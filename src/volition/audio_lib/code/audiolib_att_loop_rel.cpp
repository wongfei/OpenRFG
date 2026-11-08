// Original: volition/audio_lib/code/audiolib_att_loop_rel.cpp
//
// Attack/loop/release sounds (SFX.md SX2): foley_alr.xtbl names up to six cues per ALR (near and far attack, loop and
// release). A start plays the attack, the loop delayed by the repeat rate and the release paused (params flags 0x42) so
// it is ready on stop; a 3D ALR between the near maximum and the far minimum plays both sets, the far one on the
// crossover curve (rolloff curve 2). A stop marks the ALR; audiolib_alr_process fades the loops and unpauses the
// releases. An ALR id packs the pool index (low half) and the slot's rolling counter (high half). Compiled in
// audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_att_loop_rel.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdlib.h>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0xC
struct audiolib_alr_lookup_data_list
{
    int count; // 0x00
    int total; // 0x04
    audiolib_alr_lookup_data* lookup_list; // 0x08
};
static_assert(sizeof(audiolib_alr_lookup_data_list) == 0xC, "audiolib_alr_lookup_data_list");

// ---- public globals (rfg.pdb; no header declares them)
object_pool<audiolib_process_data,15,1> Audio_process_list(nullptr, nullptr, 9); // 0x01A7DF70
cs_wrapper Audiolib_play_cs(0); // 0x01A9D0E4

// ---- file-static globals
static int Num_timed_out_ALRs; // 0x01A79AFC
static audiolib_alr_lookup_data_list Audiolib_ALR_lookup_list; // 0x01A79CC0
static bool Audio_ALR_inited; // 0x01A79CCC
static cs_wrapper Audiolib_alr_cs(0); // 0x01A811B0
static object_pool<audiolib_alr_data,100,1> ALR_list(nullptr, &audiolib_alr_data::drop, 0x4F); // 0x01A988F0

// 0x00485CD0
audiolib_alr_params::audiolib_alr_params()
{
}

// 0x00482F50 (.h:76)
void audiolib_alr_data::invalidate()
{
}

// 0x00482F60 (.h:82)
audiolib_alr_data::audiolib_alr_data()
{
    // The rolling counter keeps 16 bits (the high half of an ALR id).
    rolling_counter &= 0xFFFF;
    if (rolling_counter == 0)
    {
        rolling_counter = 1;
    }
}

// 0x00485AD0 (:97)
void audiolib_alr_data::drop()
{
    // ALR_list's release callback: the slot's next ids no longer match the old ones.
    ++rolling_counter;
    if (rolling_counter > 0xFFFF)
    {
        rolling_counter = 1;
    }
}

// 0x0048EB90 (:108)
audiolib_alr_data* audiolib_internal_alr_id_to_ptr(int id)
{
    Audiolib_alr_cs.enter();
    if (id != -1)
    {
        const unsigned int index = id & 0xFFFF;
        if (index < 100)
        {
            audiolib_alr_data* alr = ALR_list.get_entry((int)index);
            if (alr != nullptr && (id >> 16) == alr->rolling_counter)
            {
                Audiolib_alr_cs.leave();
                return alr;
            }
        }
    }
    Audiolib_alr_cs.leave();
    return nullptr;
}

// A cue id from a table name, -1 for an empty one (audiolib_internal_init_alr_lookup_table_entry's steps).
static void audiolib_alr_init_cue_id(const char* name, int& cue_id)
{
    if (name[0] == '\0')
    {
        cue_id = -1;
    }
    else
    {
        audiolib_cue_get_id(crc_str(name, 0), &cue_id);
    }
}

// 0x0048EC00 (:134), alr_data in EAX
static void audiolib_internal_init_alr_lookup_table_entry(audiolib_alr_lookup_data* alr_data)
{
    audiolib_alr_init_cue_id(alr_data->attack_name, alr_data->params.attack_cue_id);
    audiolib_alr_init_cue_id(alr_data->loop_name, alr_data->params.loop_cue_id);
    audiolib_alr_init_cue_id(alr_data->release_name, alr_data->params.release_cue_id);
    audiolib_alr_init_cue_id(alr_data->far_attack_name, alr_data->params.far_attack_cue_id);
    audiolib_alr_init_cue_id(alr_data->far_loop_name, alr_data->params.far_loop_cue_id);
    audiolib_alr_init_cue_id(alr_data->far_release_name, alr_data->params.far_release_cue_id);
}

// 0x00485AF0 (:169)
bool audiolib_internal_alr_list_init(int total)
{
    Audiolib_alr_cs.enter();
    if (Audiolib_ALR_lookup_list.lookup_list == nullptr)
    {
        Audiolib_ALR_lookup_list.count = 0;
        Audiolib_ALR_lookup_list.lookup_list = (audiolib_alr_lookup_data*)Audiolib_state.pool->alloc(total * sizeof(audiolib_alr_lookup_data), 0);
        if (Audiolib_ALR_lookup_list.lookup_list == nullptr)
        {
            Audiolib_alr_cs.leave();
            return false;
        }
        Audiolib_ALR_lookup_list.total = total;
        memset(Audiolib_ALR_lookup_list.lookup_list, 0, total * sizeof(audiolib_alr_lookup_data));
    }
    else if (Audiolib_ALR_lookup_list.total != total)
    {
        // A second table must have the same size.
        Audiolib_alr_cs.leave();
        return false;
    }
    Audiolib_alr_cs.leave();
    return true;
}

// 0x00485B70 (:194)
audiolib_alr_lookup_data* audiolib_internal_find_alr_by_name(const char* name)
{
    for (int i = 0; i < Audiolib_ALR_lookup_list.count; ++i)
    {
        if (Audiolib_ALR_lookup_list.lookup_list != nullptr && strcmp(Audiolib_ALR_lookup_list.lookup_list[i].name, name) == 0)
        {
            return &Audiolib_ALR_lookup_list.lookup_list[i];
        }
    }
    return nullptr;
}

// An optional cue name (empty when missing).
static void audiolib_alr_parse_name(char (&name)[64], xml_element* element, const char* attribute_name)
{
    const char* value = xml_table_get_opt_string_ref(element, attribute_name);
    if (value == nullptr)
    {
        name[0] = '\0';
    }
    else
    {
        strncpy(name, value, sizeof(name));
    }
}

// 0x0048ED90 (:206)
audiolib_result audiolib_internal_parse_alr(xml_element* alr_xml_info)
{
    const char* name = xml_table_get_req_string_ref(alr_xml_info, "Name");
    if (name == nullptr)
    {
        return AUDIOLIB_ERR_FOLEY_TABLE;
    }
    // A name already in the list is replaced.
    audiolib_alr_lookup_data* alr_data = audiolib_internal_find_alr_by_name(name);
    if (alr_data == nullptr)
    {
        if (Audiolib_ALR_lookup_list.count >= Audiolib_ALR_lookup_list.total)
        {
            return AUDIOLIB_ERR_FOLEY_TABLE;
        }
        alr_data = &Audiolib_ALR_lookup_list.lookup_list[Audiolib_ALR_lookup_list.count++];
    }
    memset(alr_data, 0, sizeof(*alr_data));
    strncpy(alr_data->name, name, sizeof(alr_data->name));
    xml_element* element = xml_find(alr_xml_info, "Near");
    if (element != nullptr)
    {
        audiolib_alr_parse_name(alr_data->attack_name, element, "Near_Attack");
        audiolib_alr_parse_name(alr_data->loop_name, element, "Near_Loop");
        audiolib_alr_parse_name(alr_data->release_name, element, "Near_Release");
    }
    element = xml_find(alr_xml_info, "Far");
    if (element != nullptr)
    {
        audiolib_alr_parse_name(alr_data->far_attack_name, element, "Far_Attack");
        audiolib_alr_parse_name(alr_data->far_loop_name, element, "Far_Loop");
        audiolib_alr_parse_name(alr_data->far_release_name, element, "Far_Release");
    }
    const char* value = xml_table_get_req_string_ref(alr_xml_info, "Num_Shots");
    if (value != nullptr)
    {
        alr_data->params.num_shots = atoi(value);
    }
    value = xml_table_get_req_string_ref(alr_xml_info, "Repeat_Rate");
    if (value != nullptr)
    {
        alr_data->params.repeat_rate = atoi(value);
    }
    value = xml_table_get_req_string_ref(alr_xml_info, "IS_3D");
    if (value != nullptr && v_stricmp(value, "true") == 0)
    {
        alr_data->params.is_3d = true;
    }
    audiolib_internal_init_alr_lookup_table_entry(alr_data);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048EFB0 (:289)
audiolib_result audiolib_internal_alr_load_alr_table()
{
    audiolib_result result = AUDIOLIB_ERR_FILE_OPEN;
    char path[256];
    strncpy(path, Audiolib_state.root_dir, sizeof(path));
    strncat(path, Audiolib_state.table_dir, sizeof(path));
    strncat(path, "foley_alr.xtbl", sizeof(path));
    xml_element* root = xml_table_parse(path);
    if (root != nullptr && audiolib_internal_alr_list_init(xml_count(root, "Attack_Loop_Release")))
    {
        result = AUDIOLIB_ERR_NONE;
        for (xml_element* alr = xml_find(root, "Attack_Loop_Release"); alr != nullptr; alr = xml_find_next(root, alr, "Attack_Loop_Release"))
        {
            if (audiolib_internal_parse_alr(alr) < 0)
            {
                result = AUDIOLIB_ERR_FILE_OPEN;
                break;
            }
        }
        xml_table_close(root);
    }
    // Original: a table the list cannot take is never closed.
    Audiolib_state.number_of_ALRs = Audiolib_ALR_lookup_list.count;
    Audiolib_ALR_lookup_list.count = Audiolib_state.number_of_ALRs;
    return result;
}

// 0x00485BF0 (:322)
audiolib_alr_lookup_data* audiolib_internal_lookup_alr_params_in_list(int cue_id)
{
    if (cue_id == -1)
    {
        return nullptr;
    }
    // Any of the six cues finds its ALR.
    for (int i = 0; i < Audiolib_ALR_lookup_list.count; ++i)
    {
        audiolib_alr_lookup_data* alr = &Audiolib_ALR_lookup_list.lookup_list[i];
        const audiolib_alr_params& params = alr->params;
        if (params.attack_cue_id == cue_id || params.loop_cue_id == cue_id || params.release_cue_id == cue_id ||
            params.far_attack_cue_id == cue_id || params.far_loop_cue_id == cue_id || params.far_release_cue_id == cue_id)
        {
            return alr;
        }
    }
    return nullptr;
}

// A loop fades out over the ALR's fade time (the default fade out when negative).
static void audiolib_alr_fade_loop(const audiolib_alr_data* alr, int play_id)
{
    if (play_id != -1 && Audiolib_state.initialized && play_id != 0)
    {
        audiolib_internal_playlist_fade_out(play_id, alr->fade_time < 0 ? Audiolib_settings.default_fade_out : alr->fade_time);
    }
}

// A paused release starts (audiolib_internal_playlist_set_pause(play_id, false), inlined).
static void audiolib_alr_start_release(int play_id)
{
    if (play_id == -1 || !Audiolib_state.initialized || play_id == 0)
    {
        return;
    }
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play != nullptr && (play->action & 0x10) == 0)
    {
        unsigned short action = play->action;
        switch (play->play_state)
        {
        case AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL:
        case AUDIOLIB_HW_PLAY_STATE_LOAD_REQUEST:
        case AUDIOLIB_HW_PLAY_STATE_LOADING:
        case AUDIOLIB_HW_PLAY_STATE_LOADED:
        case AUDIOLIB_HW_PLAY_STATE_PREPARING:
            if ((play->params.flags & 2) != 0)
            {
                play->params.flags &= 0xFFFD;
                play->action = action | 2;
            }
            break;
        case AUDIOLIB_HW_PLAY_STATE_READY:
            play->params.flags &= 0xFFF9;
            play->action = (action & 0xFFFB) | 10;
            break;
        case AUDIOLIB_HW_PLAY_STATE_PAUSED:
            play->action = (action & 0xFFFB) | 8;
            break;
        default:
            break;
        }
    }
    Audiolib_play_cs.leave();
}

// 0x00499BC0 (:350)
bool audiolib_alr_thread_function(void* user_data, int curr_time, int time_step)
{
    (void)user_data;
    (void)curr_time;
    (void)time_step;
    Audiolib_alr_cs.enter();
    ALR_list.thread_access.enter();
    audiolib_alr_data* next = ALR_list.get_first(0);
    while (next != nullptr)
    {
        audiolib_alr_data* alr = next;
        next = ALR_list.get_next(alr);
        if (!alr->kill_me)
        {
            continue;
        }
        // Stopped: the loops fade, the releases start and the ALR is gone.
        audiolib_alr_fade_loop(alr, alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP]);
        audiolib_alr_fade_loop(alr, alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP]);
        audiolib_alr_start_release(alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE]);
        audiolib_alr_start_release(alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE]);
        ALR_list.free(alr);
    }
    ALR_list.thread_access.leave();
    Audiolib_alr_cs.leave();
    return false;
}

// 0x0049FE40 (:398)
bool audiolib_alr_system_init()
{
    if (Audio_ALR_inited)
    {
        return false;
    }
    Audiolib_alr_cs.enter();
    ALR_list.thread_access.enter();
    for (audiolib_alr_data* alr = ALR_list.get_first(0); alr != nullptr; alr = ALR_list.free(alr))
    {
    }
    ALR_list.thread_access.leave();
    Num_timed_out_ALRs = 0;
    audiolib_internal_alr_load_alr_table();
    // audiolib_thread_function_register, inlined.
    audiolib_process_data* process = Audio_process_list.alloc(0);
    if (process == nullptr)
    {
        Audiolib_alr_cs.leave();
        return false;
    }
    process->user_data = nullptr;
    process->function = audiolib_alr_thread_function;
    strncpy(process->func_name, "audiolib_alr_process", sizeof(process->func_name));
    Audio_ALR_inited = true;
    Audiolib_alr_cs.leave();
    return true;
}

// 0x0049FF00 (:419)
void audiolib_alr_system_shutdown()
{
    Audio_ALR_inited = false;
    audiolib_thread_function_unregister(audiolib_alr_thread_function);
    Audiolib_alr_cs.enter();
    ALR_list.thread_access.enter();
    for (audiolib_alr_data* alr = ALR_list.get_first(0); alr != nullptr; alr = ALR_list.free(alr))
    {
    }
    ALR_list.thread_access.leave();
    Audiolib_alr_cs.leave();
}

// 0x00494F50 (:430)
int audiolib_alr_get_current_alr_count()
{
    return (int)ALR_list.get_capacity() - ALR_list.get_free();
}

// 0x00485C60 (:435)
bool audiolib_att_loop_rel_is_valid(int cue_id)
{
    return Audio_ALR_inited && audiolib_internal_lookup_alr_params_in_list(cue_id) != nullptr;
}

// 0x00485C80 (:450)
audiolib_result audiolib_att_loop_rel_get_cue_id(const char* alr_name, int* cue_id)
{
    if (!Audio_ALR_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (alr_name == nullptr || cue_id == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // An ALR is known by its near loop.
    audiolib_alr_lookup_data* alr = audiolib_internal_find_alr_by_name(alr_name);
    if (alr == nullptr || alr->params.loop_cue_id == 0 || alr->params.loop_cue_id == -1)
    {
        return AUDIOLIB_ERR_CUE_NOT_FOUND;
    }
    *cue_id = alr->params.loop_cue_id;
    return AUDIOLIB_ERR_NONE;
}

static bool audiolib_alr_cue_is_set(int cue_id)
{
    return cue_id != 0 && cue_id != -1;
}

// 0x0049FF90 (:469)
audiolib_result audiolib_att_loop_rel_start(audiolib_alr_params* alr_info, int* alr_id)
{
    if (!Audio_ALR_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (alr_info == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_alr_cs.enter();
    if (alr_id != nullptr)
    {
        *alr_id = -1;
    }
    // The last free slot is kept for a 2D ALR unless one is already running.
    if (alr_info->is_3d && ALR_list.get_free() < 2)
    {
        bool found_2d = false;
        ALR_list.thread_access.enter();
        for (audiolib_alr_data* other = ALR_list.get_first(0); other != nullptr; other = ALR_list.get_next(other))
        {
            if (!other->is_3d)
            {
                found_2d = true;
                break;
            }
        }
        ALR_list.thread_access.leave();
        if (!found_2d)
        {
            Audiolib_alr_cs.leave();
            return AUDIOLIB_ERR_NO_MORE_ALR_SLOTS;
        }
    }
    audiolib_alr_data* alr = ALR_list.alloc(0);
    if (alr == nullptr)
    {
        Audiolib_alr_cs.leave();
        return AUDIOLIB_ERR_NO_MORE_ALR_SLOTS;
    }
    alr->kill_me = false;
    alr->start_time = timer_get(TIMER_MICROSECONDS);
    const int id = (ALR_list.get_index(alr) & 0xFFFF) | (alr->rolling_counter << 16);
    alr->alr_id = id;
    alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_ATTACK] = alr_info->attack_cue_id;
    alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP] = alr_info->loop_cue_id;
    alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE] = alr_info->release_cue_id;
    alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_ATTACK] = alr_info->far_attack_cue_id;
    alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP] = alr_info->far_loop_cue_id;
    alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE] = alr_info->far_release_cue_id;
    alr->is_3d = alr_info->is_3d;
    alr->num_shots = alr_info->num_shots;
    alr->repeat_rate = alr_info->repeat_rate;
    alr->type = AUDIOLIB_ALR_TYPE_NEAR;
    // The near cues reach the near loop's maximum distance, the far cues start at the far loop's minimum (1 and 1e6
    // without loops). Original: the distance is measured for 2D ALRs too.
    alr->dist = vec_dist(Audiolib_state.listener_pos, alr_info->pos);
    alr->max = 1.0f;
    alr->min = 1e+06f;
    if (audiolib_alr_cue_is_set(alr_info->loop_cue_id))
    {
        const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(alr_info->loop_cue_id);
        if (foley != nullptr)
        {
            alr->max = Audiolib_settings.min_max_distance_entries[foley->min_max_index].max;
        }
    }
    if (audiolib_alr_cue_is_set(alr_info->far_loop_cue_id))
    {
        const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(alr_info->far_loop_cue_id);
        if (foley != nullptr)
        {
            alr->min = Audiolib_settings.min_max_distance_entries[foley->min_max_index].min;
        }
    }
    alr->type = AUDIOLIB_ALR_TYPE_BOTH;
    if (alr->dist < alr->min)
    {
        alr->type = AUDIOLIB_ALR_TYPE_NEAR;
    }
    else if (alr->max < alr->dist)
    {
        alr->type = AUDIOLIB_ALR_TYPE_FAR;
    }
    // Params flag 0x200: never merged; 1: 3D; 0x10: the far cue (no low pass).
    audiolib_params params;
    params.flags = alr->is_3d ? 0x201 : 0x200;
    params.pos = alr_info->pos;
    params.orient = alr_info->orient;
    bool attack_played = false;
    bool loop_played = false;
    bool release_played = false;
    if (audiolib_alr_cue_is_set(alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_ATTACK]) || audiolib_alr_cue_is_set(alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_ATTACK]))
    {
        params.rolloff_curve = 0;
        alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_ATTACK] = audiolib_cue_play(alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_ATTACK], &params, nullptr);
        if (alr->type > AUDIOLIB_ALR_TYPE_NEAR)
        {
            if (alr->type == AUDIOLIB_ALR_TYPE_BOTH)
            {
                params.curve_midpoint = alr->max;
                params.rolloff_curve = 2;
            }
            params.flags |= 0x10;
            alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_ATTACK] = audiolib_cue_play(alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_ATTACK], &params, nullptr);
        }
        if (alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_ATTACK] != -1 || alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_ATTACK] != -1)
        {
            attack_played = true;
        }
    }
    if (!audiolib_alr_cue_is_set(alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP]) && !audiolib_alr_cue_is_set(alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP]))
    {
        alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP] = -1;
        alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP] = -1;
    }
    else
    {
        if (alr->is_3d)
        {
            params.flags |= 1;
        }
        // The loop starts at a random shot (CRT rand(), SFX.md 2.5), after the attack's first shot.
        if (alr->num_shots > 0)
        {
            params.offset_ms = alr->repeat_rate * (rand() % alr->num_shots);
        }
        params.delay_ms = attack_played ? alr->repeat_rate : 0;
        params.rolloff_curve = 0;
        alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP] = audiolib_cue_play(alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP], &params, nullptr);
        if (alr->type > AUDIOLIB_ALR_TYPE_NEAR)
        {
            if (alr->type == AUDIOLIB_ALR_TYPE_BOTH)
            {
                params.curve_midpoint = alr->max;
                params.rolloff_curve = 2;
            }
            params.flags |= 0x10;
            alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP] = audiolib_cue_play(alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP], &params, nullptr);
        }
        if (alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_LOOP] != -1 || alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_LOOP] != -1)
        {
            loop_played = true;
        }
    }
    if (!audiolib_alr_cue_is_set(alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE]) && !audiolib_alr_cue_is_set(alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE]))
    {
        alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE] = -1;
        alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE] = -1;
    }
    else
    {
        // After an attack or loop the release waits paused (2) and distance-kept (0x40) for the stop; alone it plays.
        params.flags = attack_played || loop_played ? 0x42 : 0;
        if (alr->is_3d)
        {
            params.flags |= 1;
        }
        params.offset_ms = 0;
        params.delay_ms = 0;
        params.rolloff_curve = 0;
        alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE] = audiolib_cue_play(alr->cue_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE], &params, nullptr);
        if (alr->type > AUDIOLIB_ALR_TYPE_NEAR)
        {
            if (alr->type == AUDIOLIB_ALR_TYPE_BOTH)
            {
                params.curve_midpoint = alr->max;
                params.rolloff_curve = 2;
            }
            params.flags |= 0x10;
            alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE] = audiolib_cue_play(alr->cue_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE], &params, nullptr);
        }
        // Original: only a 2D near release on its own counts (the flags are 0 only then).
        if (params.flags == 0 && (alr->play_ids[AUDIOLIB_ALR_INDEX_NEAR_RELEASE] != -1 || alr->play_ids[AUDIOLIB_ALR_INDEX_FAR_RELEASE] != -1))
        {
            release_played = true;
        }
    }
    if (!attack_played && !loop_played && !release_played)
    {
        ALR_list.free(alr);
        Audiolib_alr_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // Original: alr_id is written without the null check above.
    *alr_id = id;
    Audiolib_alr_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004A04C0 (:732)
audiolib_result audiolib_att_loop_rel_start(int cue_id, int* alr_id, bool skip_attack_sound, const vector* pos, const vector* orient)
{
    if (!Audio_ALR_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    const audiolib_alr_lookup_data* lookup = audiolib_internal_lookup_alr_params_in_list(cue_id);
    if (lookup != nullptr)
    {
        audiolib_alr_params params = lookup->params;
        if (skip_attack_sound)
        {
            params.attack_cue_id = -1;
            params.far_attack_cue_id = -1;
        }
        if (params.is_3d && pos != nullptr)
        {
            params.pos = *pos;
            if (orient != nullptr)
            {
                params.orient = *orient;
            }
        }
        else
        {
            params.is_3d = false;
        }
        return audiolib_att_loop_rel_start(&params, alr_id);
    }
    // Not an ALR: a one-shot cue just plays (still a bad parameter for the caller).
    audiolib_cue_info info;
    if (audiolib_cue_get_info(cue_id, &info) >= 0 && info.loop_count < 1)
    {
        audiolib_params params;
        if (pos != nullptr)
        {
            params.flags |= 1;
            params.pos = *pos;
            if (orient != nullptr)
            {
                params.orient = *orient;
            }
        }
        audiolib_cue_play(cue_id, &params, nullptr);
    }
    return AUDIOLIB_ERR_BAD_PARAM;
}

// 0x004A0680 (:806)
audiolib_result audiolib_att_loop_rel_start(const char* alr_name, int* alr_id, bool skip_attack_sound, const vector* pos, const vector* orient)
{
    if (!Audio_ALR_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    const audiolib_alr_lookup_data* lookup = audiolib_internal_find_alr_by_name(alr_name);
    if (lookup != nullptr)
    {
        audiolib_alr_params params = lookup->params;
        if (skip_attack_sound)
        {
            params.attack_cue_id = -1;
            params.far_attack_cue_id = -1;
        }
        if (params.is_3d && pos != nullptr)
        {
            // Original: the table's params are copied again here, so a 3D ALR started by name keeps its attack.
            params = lookup->params;
        }
        if (pos != nullptr)
        {
            params.pos = *pos;
        }
        if (orient != nullptr)
        {
            params.orient = *orient;
        }
        return audiolib_att_loop_rel_start(&params, alr_id);
    }
    // Not an ALR: the cue of that name plays (a bad parameter for the caller).
    audiolib_params params;
    if (pos != nullptr)
    {
        params.flags |= 1;
        params.pos = *pos;
        if (orient != nullptr)
        {
            params.orient = *orient;
        }
    }
    // Original: the cue id goes into the alr_id argument's stack slot; without a name the pointer is played as an id.
    int cue_id = (int)(uintptr_t)alr_id;
    if (alr_name != nullptr)
    {
        audiolib_cue_get_id(crc_str(alr_name, 0), &cue_id);
    }
    audiolib_cue_play(cue_id, &params, nullptr);
    return AUDIOLIB_ERR_BAD_PARAM;
}

// 0x00494F60 (:878)
audiolib_result audiolib_att_loop_rel_set_pos(int alr_id, const vector* pos, const vector* orient)
{
    if (!Audio_ALR_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (pos == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_alr_cs.enter();
    audiolib_alr_data* alr = audiolib_internal_alr_id_to_ptr(alr_id);
    if (alr == nullptr)
    {
        Audiolib_alr_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // Plays that ended are forgotten.
    for (int& play_id : alr->play_ids)
    {
        if (!Audiolib_state.initialized || play_id == 0 || play_id == -1 || audiolib_internal_playlist_get_state(play_id, nullptr) != AUDIOLIB_ERR_NONE)
        {
            play_id = -1;
        }
        else
        {
            audiolib_internal_playlist_set_3d_position(play_id, pos, orient, nullptr);
        }
    }
    Audiolib_alr_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x0048F0D0 (:902)
audiolib_result audiolib_att_loop_rel_stop(int alr_id, const int fade_time_ms)
{
    if (!Audio_ALR_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_alr_cs.enter();
    audiolib_alr_data* alr = audiolib_internal_alr_id_to_ptr(alr_id);
    if (alr == nullptr)
    {
        Audiolib_alr_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // The loops fade over at most the repeat rate.
    alr->kill_me = true;
    alr->fade_time = fade_time_ms;
    if (alr->repeat_rate < fade_time_ms)
    {
        alr->fade_time = alr->repeat_rate;
    }
    Audiolib_alr_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00495020 (:934)
void audiolib_internal_att_loop_rel_kill_all()
{
    Audiolib_alr_cs.enter();
    ALR_list.thread_access.enter();
    for (audiolib_alr_data* alr = ALR_list.get_first(0); alr != nullptr; alr = ALR_list.get_next(alr))
    {
        alr->kill_me = true;
        alr->fade_time = alr->repeat_rate;
    }
    ALR_list.thread_access.leave();
    Audiolib_alr_cs.leave();
}
