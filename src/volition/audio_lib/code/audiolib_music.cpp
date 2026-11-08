// Original: volition/audio_lib/code/audiolib_music.cpp
//
// Interactive music (SFX.md SX2): rfg_music.xtbl lists the music tables (<name>.mtbl), each a score of clips (cues)
// and states. A state is "<from> << 4 | <to>" (low nibble 1 silence, 2 ambience, 4 light combat, 5 its lull, 6 heavy
// combat, 7 its lull; the home state has both nibbles equal). A playing score keeps the current clip and the next one
// (cued paused); the marker callbacks switch them ('E' end, 'F' fade, 'T' transition). Music file states are the pool
// categories (audiolib_music_load_state). Action bits: 2 start, 0x10 stop. A music id packs the pool index (low half)
// and the slot's rolling counter (high half). Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_music.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_enums.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/audio_lib/code/audiolib_markers.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0x8
struct audiolib_music_transition_string_lookup
{
    AUDIOLIB_MUSIC_STATE state; // 0x00
    const char* string; // 0x04
};
static_assert(sizeof(audiolib_music_transition_string_lookup) == 0x8, "audiolib_music_transition_string_lookup");

// ---- file-static globals
static audiolib_music_transition_string_lookup music_transition_strings[26] = { // 0x014F9358
    {AUDIOLIB_MUSIC_STATE_UNKNOWN, "UNKNOWN"},
    {AUDIOLIB_MUSIC_STATE_SILENCE, "SILENCE"},
    {AUDIOLIB_MUSIC_STATE_SILENCE_TO_AMBIENCE, "SILENCE_TO_AMBIENCE"},
    {AUDIOLIB_MUSIC_STATE_SILENCE_TO_COMBAT_LIGHT, "SILENCE_TO_COMBAT_LIGHT"},
    {AUDIOLIB_MUSIC_STATE_AMBIENCE, "AMBIENCE"},
    {AUDIOLIB_MUSIC_STATE_AMBIENCE_TO_SILENCE, "AMBIENCE_TO_SILENCE"},
    {AUDIOLIB_MUSIC_STATE_AMBIENCE_TO_COMBAT_LIGHT, "AMBIENCE_TO_COMBAT_LIGHT"},
    {AUDIOLIB_MUSIC_STATE_AMBIENCE_TO_COMBAT_HEAVY, "AMBIENCE_TO_COMBAT_HEAVY"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT, "COMBAT_LIGHT"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_TO_SILENCE, "COMBAT_LIGHT_TO_SILENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_TO_AMBIENCE, "COMBAT_LIGHT_TO_AMBIENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_TO_COMBAT_LIGHT_LULL, "COMBAT_LIGHT_TO_COMBAT_LIGHT_LULL"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_TO_COMBAT_HEAVY, "COMBAT_LIGHT_TO_COMBAT_HEAVY"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_LULL, "COMBAT_LIGHT_LULL"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_LULL_TO_SILENCE, "COMBAT_LIGHT_LULL_TO_SILENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_LULL_TO_AMBIENCE, "COMBAT_LIGHT_LULL_TO_AMBIENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT_LULL_TO_COMBAT_LIGHT, "COMBAT_LIGHT_LULL_TO_COMBAT_LIGHT"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY, "COMBAT_HEAVY"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_TO_SILENCE, "COMBAT_HEAVY_TO_SILENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_TO_AMBIENCE, "COMBAT_HEAVY_TO_AMBIENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_TO_COMBAT_LIGHT, "COMBAT_HEAVY_TO_COMBAT_LIGHT"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_TO_COMBAT_HEAVY_LULL, "COMBAT_HEAVY_TO_COMBAT_HEAVY_LULL"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_LULL, "COMBAT_HEAVY_LULL"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_LULL_TO_SILENCE, "COMBAT_HEAVY_LULL_TO_SILENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_LULL_TO_AMBIENCE, "COMBAT_HEAVY_LULL_TO_AMBIENCE"},
    {AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY_LULL_TO_COMBAT_HEAVY, "COMBAT_HEAVY_LULL_TO_COMBAT_HEAVY"},
};
// audiolib_interactive_music_data Audiolib_Music (0x01A789C8) is used here too; placed in audiolib.cpp.
static bool Audiolib_Reboot_Music_System; // 0x01A7AEE5
static bool Audiolib_Music_Level_Limited; // 0x01A7AEE6
static int Audiolib_Music_Fade_Time; // 0x01A7AEE8
static object_pool<audiolib_music_file_data,20,8> Music_list(nullptr, &audiolib_music_file_data::drop, 0x44); // 0x01A7AF20
static cs_wrapper Audiolib_music_cs(0); // 0x01A7DF2C
static cs_wrapper Audiolib_music_old_clip_list_cs(0); // 0x01A97B5C
// lf_node<cpu_job> Cpu_jobs[1536] (0x01D3E0A8) is used here too; placed in scheduler.cpp.


static AUDIOLIB_MUSIC_STATE audiolib_music_state(int value)
{
    return (AUDIOLIB_MUSIC_STATE)value;
}

// The state named in text (the last table entry whose name it contains), UNKNOWN for none (the inlined lookup of
// several functions).
static AUDIOLIB_MUSIC_STATE audiolib_music_state_from_text(const char* text)
{
    for (int i = 25; i >= 0; --i)
    {
        if (strstr(text, music_transition_strings[i].string) != nullptr)
        {
            return music_transition_strings[i].state;
        }
    }
    return AUDIOLIB_MUSIC_STATE_UNKNOWN;
}

// The clip of a name crc in the score (audiolib_internal_music_find_clip_by_name's loop, inlined in the parse).
static audiolib_music_clip_data* audiolib_music_find_clip_by_crc(audiolib_music_score_data* score, unsigned int crc)
{
    audiolib_music_clip_data* clip = score->clips;
    for (int i = score->num_clips; i != 0; --i, ++clip)
    {
        if (crc == clip->name_crc)
        {
            return clip;
        }
    }
    return nullptr;
}

// The clip named after the ':' of text (or all of text), null without text.
static audiolib_music_clip_data* audiolib_music_find_clip_after_colon(audiolib_music_score_data* score, const char* text)
{
    const char* name = strstr(text, ":");
    name = name == nullptr ? text : name + 1;
    return name != nullptr ? audiolib_music_find_clip_by_crc(score, crc_str(name, 0)) : nullptr;
}

// 0x00482F80 (.h:225)
void audiolib_music_file_data::invalidate()
{
}

// 0x00482F90 (.h:231)
audiolib_music_file_data::audiolib_music_file_data()
{
    // The rolling counter keeps 16 bits (the high half of a music id).
    rolling_counter &= 0xFFFF;
    if (rolling_counter == 0)
    {
        rolling_counter = 1;
    }
}

// 0x00488760 (:250)
void audiolib_music_file_data::drop()
{
    // Music_list's release callback.
    ++rolling_counter;
    if (rolling_counter > 0xFFFF)
    {
        rolling_counter = 1;
    }
}

// 0x00495500 (:261)
static audiolib_music_file_data* audiolib_internal_find_music_table(const char* file_name)
{
    if (file_name == nullptr)
    {
        return nullptr;
    }
    for (int state = 0; state < NUM_AUDIOLIB_MUSIC_LOAD_STATES; ++state)
    {
        Music_list.thread_access.enter();
        for (audiolib_music_file_data* file = Music_list.get_first(state); file != nullptr; file = Music_list.get_next(file))
        {
            if (v_stricmp(file->name, file_name) == 0)
            {
                Music_list.thread_access.leave();
                return file;
            }
        }
        Music_list.thread_access.leave();
    }
    return nullptr;
}

// 0x00488780 (:327)
static AUDIOLIB_MUSIC_STATE audiolib_internal_music_convert_state_name_to_enum(const char* name, const char* clip_name)
{
    AUDIOLIB_MUSIC_STATE state = AUDIOLIB_MUSIC_STATE_UNKNOWN;
    if (name != nullptr)
    {
        state = audiolib_music_state_from_text(name);
    }
    // A heavy combat lull clip filed under ambience (0x22) belongs to heavy combat.
    if (clip_name != nullptr && state == 0x22 && strstr(clip_name, "COMBAT_HEAVY_LULL") != nullptr)
    {
        return AUDIOLIB_MUSIC_STATE_COMBAT_HEAVY;
    }
    return state;
}

// 0x004887F0 (:358), score in EDX, state_type in EAX
static audiolib_music_state_data* audiolib_internal_music_get_state_ptr_from_type(audiolib_music_score_data* score, AUDIOLIB_MUSIC_STATE state_type)
{
    // The home state of a transition state ("from" in both nibbles); the lulls share their combat state.
    unsigned int home = ((int)state_type >> 4 & 0xF) | (state_type & 0xF0);
    if (home == 0x77)
    {
        home = 0x66;
    }
    else if (home == 0x55)
    {
        home = 0x44;
    }
    if (score == nullptr || score->num_states == 0)
    {
        return nullptr;
    }
    for (int i = 0; i < score->num_states; ++i)
    {
        if ((unsigned int)score->state_data[i].state_type == home)
        {
            return &score->state_data[i];
        }
    }
    return nullptr;
}

// 0x00488850 (:387)
static audiolib_music_state_data* audiolib_internal_music_find_state_ptr_by_name(audiolib_music_score_data* score, const char* text)
{
    return audiolib_internal_music_get_state_ptr_from_type(score, text != nullptr ? audiolib_music_state_from_text(text) : AUDIOLIB_MUSIC_STATE_UNKNOWN);
}

// 0x004955C0 (:400)
static audiolib_music_file_data* audiolib_internal_music_id_to_ptr(int music_id)
{
    const unsigned int index = music_id & 0xFFFF;
    if (index >= 20)
    {
        return nullptr;
    }
    audiolib_music_file_data* file = Music_list.get_entry((int)index);
    return file != nullptr && (music_id >> 16) == file->rolling_counter ? file : nullptr;
}

// A random first clip of a state (vrand).
static audiolib_music_clip_data* audiolib_music_random_first_clip(audiolib_music_state_data* state)
{
    return state->first_clips[vrand() % (unsigned int)state->num_first_clips];
}

// 0x0048FBE0 (:428)
static audiolib_music_clip_data* audiolib_internal_music_find_transition_clip(audiolib_music_score_data* score, AUDIOLIB_MUSIC_STATE to_state)
{
    if (score == nullptr)
    {
        return nullptr;
    }
    bool to_lull = false;
    int from = score->curr_state & 0xF0;
    int to = to_state;
    // A lull goes to its combat state's home.
    if (to == 0x70 || to == 0x50)
    {
        to = to == 0x70 ? 0x60 : 0x40;
        to_lull = true;
        from = to;
    }
    bool start_fresh = false;
    audiolib_music_clip_data* from_clip;
    audiolib_music_state_data* from_state;
    if (from == 0x20 && to == 0x60)
    {
        // Ambience to heavy combat goes through a light combat first clip.
        from = 0x40;
        const int index = audiolib_internal_music_get_first_clip_index(score, AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT);
        from_state = audiolib_internal_music_get_state_ptr_from_type(score, AUDIOLIB_MUSIC_STATE_COMBAT_LIGHT);
        from_clip = from_state->first_clips[index];
    }
    else
    {
        const int from_target = score->curr_state & 0xF;
        if (from >= 0x60 || (from_target != 4 && from_target * 0x10 > 0x3F))
        {
            // From combat, or heading for heavy combat: to light combat starts afresh.
            start_fresh = to == 0x40;
        }
        from_clip = score->curr_clip;
        from_state = audiolib_internal_music_get_state_ptr_from_type(score, audiolib_music_state(from));
    }
    audiolib_music_state_data* to_state_data = audiolib_internal_music_get_state_ptr_from_type(score, audiolib_music_state(to));
    if (from_state == nullptr || to_state_data == nullptr)
    {
        return nullptr;
    }
    const int to_target = to >> 4;
    if (start_fresh && to_state_data->transitions != nullptr)
    {
        audiolib_music_clip_data* clip = audiolib_music_random_first_clip(to_state_data);
        if (clip != nullptr)
        {
            clip->my_state = audiolib_music_state((to_target & 0xF) | from);
            return clip;
        }
    }
    audiolib_music_transition_data* from_transitions = from_state->transitions;
    audiolib_music_transition_data* transition = nullptr;
    if (to_lull && from == to)
    {
        // Into a lull: a lull clip of a first clip.
        if (from_transitions != nullptr)
        {
            audiolib_music_clip_data* clip = audiolib_music_random_first_clip(from_state);
            audiolib_music_clip_data* lull = clip;
            if (clip != nullptr && clip->num_lulls != 0)
            {
                lull = clip->lull_clips[vrand() % (unsigned int)clip->num_lulls];
                if (lull != nullptr)
                {
                    lull->my_state = audiolib_music_state(from >> 4 | from);
                    lull->parent_clip = clip;
                }
            }
            return lull;
        }
    }
    else if (from_transitions != nullptr)
    {
        if (from_clip == nullptr)
        {
            goto by_state;
        }
        // Rising: a clip transition of the current state from this clip.
        if (from < to && score->curr_state < 0x45)
        {
            audiolib_music_transition_data* candidate = from_transitions;
            for (int i = from_state->num_transitions; i > 0; --i, ++candidate)
            {
                if (candidate->trans_type == AUDIOLIB_MUSIC_TRANSITION_TYPE_CLIP && (candidate->from_clip->my_state & 0xF0) == from &&
                    candidate->from_clip == from_clip)
                {
                    transition = candidate;
                    goto found;
                }
            }
        }
    }
    if (from_clip != nullptr && to > 0x20 && to_state_data->transitions != nullptr)
    {
        // A clip transition of the target state from this clip.
        audiolib_music_transition_data* candidate = to_state_data->transitions;
        for (int i = to_state_data->num_transitions; i > 0; --i, ++candidate)
        {
            if (candidate->trans_type == AUDIOLIB_MUSIC_TRANSITION_TYPE_CLIP && candidate->from_clip == from_clip)
            {
                transition = candidate;
                goto found;
            }
        }
    }
by_state:
    {
        // A state transition of the target state from the current state, or a first clip when staying.
        audiolib_music_transition_data* candidate = to_state_data->transitions;
        for (int i = to_state_data->num_transitions; i >= 1; --i, ++candidate)
        {
            if (candidate->trans_type == AUDIOLIB_MUSIC_TRANSITION_TYPE_STATE && (candidate->from_state & 0xF0) == from)
            {
                transition = candidate;
                goto found;
            }
        }
        audiolib_music_clip_data* clip = nullptr;
        if (from == to && from_transitions != nullptr)
        {
            clip = audiolib_music_random_first_clip(from_state);
            if (clip != nullptr)
            {
                clip->my_state = audiolib_music_state(from >> 4 | from);
            }
        }
        return clip;
    }
found:
    audiolib_music_clip_data* clip = transition->clips[vrand() % (unsigned int)transition->num_clips];
    if (clip == nullptr)
    {
        return nullptr;
    }
    if (transition->trans_type != AUDIOLIB_MUSIC_TRANSITION_TYPE_STATE)
    {
        clip->my_state = audiolib_music_state((to_target & 0xF) | from);
    }
    else
    {
        clip->my_state = audiolib_music_state((to_target & 0xF) | (to & 0xF0));
    }
    return clip;
}

// 0x004888B0 (:644), score in EDI
static audiolib_music_clip_data* audiolib_internal_music_get_exit_cue(audiolib_music_score_data* score)
{
    if (score == nullptr)
    {
        return nullptr;
    }
    // Original: no check for a missing state.
    audiolib_music_state_data* state = audiolib_internal_music_get_state_ptr_from_type(score, score->curr_state);
    const int num_decisions = state->exit.num_decisions;
    if (num_decisions == 0)
    {
        return nullptr;
    }
    audiolib_music_clip_data* clip = num_decisions < 2 ? state->exit.decisions[0].dest_clip
                                                       : state->exit.decisions[vrand() % (unsigned int)state->exit.num_decisions].dest_clip;
    clip->my_state = audiolib_music_state((score->curr_state >> 4 & 0xF) | (score->curr_state & 0xF0));
    return clip;
}

// 0x00488910 (:679), score_ptr in ESI, name in EAX
static audiolib_music_clip_data* audiolib_internal_music_find_clip_by_name(audiolib_music_score_data* score_ptr, const char* name)
{
    if (score_ptr == nullptr || name == nullptr)
    {
        return nullptr;
    }
    return audiolib_music_find_clip_by_crc(score_ptr, crc_str(name, 0));
}

// 0x0048FED0 (:707)
audiolib_result audiolib_internal_parse_music_table(audiolib_music_file_data* file, xml_element* music)
{
    if (file == nullptr || music == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // The score is one block: the score, its clips, then lull lists, decisions, states, first clips, transitions and
    // their clip lists in parse order. The size counts (in 4-byte units) are rfg.exe's. Original: the exit decisions
    // are counted on a null element (0), and only the last state's first clips are counted (each needs 2 entries); the
    // 30 spare entries absorb that.
    int num_clips = 0;
    int num_decisions = 0;
    int num_lulls = 0;
    int num_states = 1;
    int num_transitions = 0;
    int num_transition_clips = 0;
    int num_exits = 0;
    int num_first_clips = 0;
    xml_element* score_xml = xml_find(music, "SCORE");
    if (score_xml != nullptr)
    {
        for (xml_element* clip = xml_find(score_xml, "CLIP"); clip != nullptr; clip = xml_find_next(score_xml, clip, "CLIP"))
        {
            ++num_clips;
            num_decisions += xml_count(clip, "LOOP") + xml_count(clip, "WEIGHTED") + xml_count(clip, "ALWAYS");
            num_lulls += xml_count(clip, "LULL_NAME");
        }
        for (xml_element* state = xml_find(score_xml, "STATE"); state != nullptr; state = xml_find_next(score_xml, state, "STATE"))
        {
            ++num_states;
            num_first_clips = xml_count(state, "FIRST_CLIP");
            for (xml_element* transition = xml_find(state, "TRANSITION"); transition != nullptr; transition = xml_find_next(state, transition, "TRANSITION"))
            {
                ++num_transitions;
                num_transition_clips += xml_count(transition, "CLIP");
            }
            for (xml_element* transition = xml_find(state, "TRANSITION_CLIP"); transition != nullptr;
                 transition = xml_find_next(state, transition, "TRANSITION_CLIP"))
            {
                ++num_transitions;
                num_transition_clips += xml_count(transition, "CLIP");
            }
            for (xml_element* exit = xml_find(state, "EXIT_TRACKS"); exit != nullptr; exit = xml_find_next(state, exit, "EXIT_TRACKS"))
            {
                ++num_exits;
                num_transition_clips += xml_count(nullptr, "WEIGHTED");
            }
        }
    }
    const unsigned int size = (num_states * 9 + num_transition_clips * 4 + 30 + num_transitions * 6 + num_lulls + (num_decisions + num_clips * 2) * 4 +
                               num_exits + num_first_clips) *
                              4;
    audiolib_music_score_data* score = (audiolib_music_score_data*)Audiolib_state.pool->alloc(size, 0);
    if (score == nullptr)
    {
        keen::debug::nativePrintArgs("Couldn't allocate space for music table data!");
        return AUDIOLIB_ERR_MEM_ALLOC_FAILED;
    }
    memset(score, 0, size);
    Audiolib_state.music_data_size = size;
    file->score = score;
    score_xml = xml_find(music, "SCORE");
    score->name_crc = crc_str(xml_table_get_req_string_ref(score_xml, "NAME"), 0);
    const short clip_count = (short)xml_count(score_xml, "CLIP");
    score->clips = (audiolib_music_clip_data*)(score + 1);
    char* cursor = (char*)(score->clips + clip_count);
    score->num_clips = clip_count;
    // Clips: name, cue (by CUE_NAME), tempo (120) and beats per bar (4).
    audiolib_music_clip_data* clip = score->clips;
    for (xml_element* clip_xml = xml_find(score_xml, "CLIP"); clip_xml != nullptr; clip_xml = xml_find_next(score_xml, clip_xml, "CLIP"), ++clip)
    {
        clip->num_decisions = 0;
        clip->name_crc = crc_str(xml_table_get_req_string_ref(clip_xml, "NAME"), 0);
        const char* cue_name = xml_table_get_req_string_ref(clip_xml, "CUE_NAME");
        if (cue_name != nullptr)
        {
            const unsigned int cue_crc = crc_str(cue_name, 0);
            if (Audiolib_state.initialized)
            {
                // audiolib_cue_get_id, inlined.
                clip->cue_id = -1;
                const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(cue_crc, nullptr));
                if (foley != nullptr)
                {
                    clip->cue_id = (int)((unsigned int)(foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)foley << 16);
                }
            }
        }
        const char* tempo = xml_table_get_opt_string_ref(clip_xml, "TEMPO");
        clip->tempo = tempo == nullptr ? 120 : (unsigned char)atoi(tempo);
        const char* beats = xml_table_get_opt_string_ref(clip_xml, "INTEGRAL_BEATS");
        clip->integral_beat = beats == nullptr ? 4 : (unsigned char)atoi(beats);
    }
    // Lulls and decisions.
    clip = score->clips;
    for (xml_element* clip_xml = xml_find(score_xml, "CLIP"); clip_xml != nullptr; clip_xml = xml_find_next(score_xml, clip_xml, "CLIP"), ++clip)
    {
        const unsigned char lull_count = (unsigned char)xml_count(clip_xml, "LULL_NAME");
        clip->num_lulls = lull_count;
        clip->lull_clips = nullptr;
        if (lull_count != 0)
        {
            clip->lull_clips = (audiolib_music_clip_data**)cursor;
            audiolib_music_clip_data** lull = clip->lull_clips;
            cursor += lull_count * sizeof(audiolib_music_clip_data*);
            for (xml_element* lull_xml = xml_find(clip_xml, "LULL_NAME"); lull_xml != nullptr; lull_xml = xml_find_next(clip_xml, lull_xml, "LULL_NAME"))
            {
                // Original: a name without ':' is taken from the clip element's text.
                const char* name = strstr(lull_xml->text, ":");
                name = name == nullptr ? clip_xml->text : name + 1;
                audiolib_music_clip_data* lull_clip = name != nullptr ? audiolib_music_find_clip_by_crc(score, crc_str(name, 0)) : nullptr;
                *lull = lull_clip;
                if (lull_clip == nullptr)
                {
                    // Unknown lulls are dropped from the count.
                    if (clip->num_lulls != 0)
                    {
                        --clip->num_lulls;
                    }
                }
                else
                {
                    ++lull;
                }
            }
        }
        unsigned char total_weights = 0;
        const unsigned char decision_count =
            (unsigned char)((unsigned char)xml_count(clip_xml, "ALWAYS") + (unsigned char)xml_count(clip_xml, "LOOP") + (unsigned char)xml_count(clip_xml, "WEIGHTED"));
        clip->num_decisions = decision_count;
        clip->last_decision = -1;
        if (decision_count == 0)
        {
            clip->decisions = nullptr;
        }
        else
        {
            clip->decisions = (audiolib_music_clip_decision_data*)cursor;
            audiolib_music_clip_decision_data* decision = clip->decisions;
            cursor += decision_count * sizeof(audiolib_music_clip_decision_data);
            // LOOP "<min>[:<max>]": repeat the clip.
            xml_element* loop = xml_find(clip_xml, "LOOP");
            if (loop != nullptr)
            {
                const short loops = (short)atoi(loop->text);
                if (loops > 0)
                {
                    decision->type = AUDIOLIB_MUSIC_CUE_DECISION_TYPE_LOOP;
                    decision->value = (short)atoi(loop->text);
                    const char* max = strstr(loop->text, ":");
                    decision->max = max == nullptr ? decision->value : atoi(max + 1);
                    decision->value = loops;
                    decision->count = 0;
                    ++decision;
                }
            }
            // WEIGHTED "<weight>:<clip>".
            for (xml_element* weighted = xml_find(clip_xml, "WEIGHTED"); weighted != nullptr; weighted = xml_find_next(clip_xml, weighted, "WEIGHTED"))
            {
                const char* name = weighted->text;
                if (name == nullptr)
                {
                    return AUDIOLIB_ERR_FILE_FORMAT;
                }
                while (*name != '\0' && *name != ':')
                {
                    ++name;
                }
                if (*name == ':')
                {
                    ++name;
                }
                decision->type = AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED;
                decision->value = (short)atoi(weighted->text);
                total_weights = (unsigned char)(total_weights + decision->value);
                decision->count = 0;
                decision->dest_clip = audiolib_music_find_clip_by_crc(score, crc_str(name, 0));
                ++decision;
            }
            // ALWAYS "<clip>": weight 10, the only weight.
            xml_element* always = xml_find(clip_xml, "ALWAYS");
            if (always != nullptr)
            {
                decision->type = AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED;
                decision->dest_clip = always->text != nullptr ? audiolib_music_find_clip_by_crc(score, crc_str(always->text, 0)) : nullptr;
                decision->value = 10;
                decision->count = 0;
                total_weights = 10;
            }
        }
        clip->total_weights = total_weights;
    }
    // States: entry 0 is a plain silence state.
    const unsigned char state_count = (unsigned char)(xml_count(score_xml, "STATE") + 1);
    score->state_data = (audiolib_music_state_data*)cursor;
    score->num_states = state_count;
    memset(score->state_data, 0, sizeof(audiolib_music_state_data));
    score->state_data[0].state_type = AUDIOLIB_MUSIC_STATE_SILENCE;
    cursor += state_count * sizeof(audiolib_music_state_data);
    audiolib_music_state_data* state = score->state_data;
    for (xml_element* state_xml = xml_find(score_xml, "STATE"); state_xml != nullptr; state_xml = xml_find_next(score_xml, state_xml, "STATE"))
    {
        ++state;
        const char* name = xml_table_get_req_string_ref(state_xml, "NAME");
        if (name == nullptr)
        {
            return AUDIOLIB_ERR_FILE_FORMAT;
        }
        state->state_type = audiolib_music_state_from_text(name);
        state->first_clips = nullptr;
        state->num_first_clips = xml_count(state_xml, "FIRST_CLIP");
        if (state->num_first_clips != 0)
        {
            const int bytes = state->num_first_clips * 4;
            state->first_clips = (audiolib_music_clip_data**)cursor;
            state->first_clip_usage_counts = (int*)(cursor + bytes);
            cursor += bytes * 2;
            memset(state->first_clip_usage_counts, 0, bytes);
            audiolib_music_clip_data** first = state->first_clips;
            for (xml_element* first_xml = xml_find(state_xml, "FIRST_CLIP"); first_xml != nullptr; first_xml = xml_find_next(state_xml, first_xml, "FIRST_CLIP"))
            {
                *first++ = first_xml->text != nullptr ? audiolib_music_find_clip_by_crc(score, crc_str(first_xml->text, 0)) : nullptr;
            }
        }
        state->transitions = nullptr;
        state->num_transitions = xml_count(state_xml, "TRANSITION") + xml_count(state_xml, "TRANSITION_CLIP");
        if (state->num_transitions != 0)
        {
            // Clip transitions first, then state transitions (their "from" states are filled in below).
            state->transitions = (audiolib_music_transition_data*)cursor;
            audiolib_music_transition_data* transition = state->transitions;
            cursor += state->num_transitions * sizeof(audiolib_music_transition_data);
            for (xml_element* transition_xml = xml_find(state_xml, "TRANSITION_CLIP"); transition_xml != nullptr;
                 transition_xml = xml_find_next(state_xml, transition_xml, "TRANSITION_CLIP"), ++transition)
            {
                transition->trans_type = AUDIOLIB_MUSIC_TRANSITION_TYPE_CLIP;
                transition->this_state = state->state_type;
                const char* from_clip = xml_table_get_req_string_ref(transition_xml, "FROM_CLIP");
                if (from_clip == nullptr)
                {
                    return AUDIOLIB_ERR_FILE_FORMAT;
                }
                transition->from_clip = audiolib_music_find_clip_by_crc(score, crc_str(from_clip, 0));
                transition->clips = nullptr;
                transition->num_clips = xml_count(transition_xml, "CLIP");
                if (transition->num_clips != 0)
                {
                    transition->clips = (audiolib_music_clip_data**)cursor;
                    audiolib_music_clip_data** target = transition->clips;
                    cursor += transition->num_clips * sizeof(audiolib_music_clip_data*);
                    for (xml_element* clip_xml = xml_find(transition_xml, "CLIP"); clip_xml != nullptr; clip_xml = xml_find_next(transition_xml, clip_xml, "CLIP"))
                    {
                        *target++ = audiolib_music_find_clip_after_colon(score, clip_xml->text);
                    }
                }
            }
            for (xml_element* transition_xml = xml_find(state_xml, "TRANSITION"); transition_xml != nullptr;
                 transition_xml = xml_find_next(state_xml, transition_xml, "TRANSITION"), ++transition)
            {
                transition->trans_type = AUDIOLIB_MUSIC_TRANSITION_TYPE_STATE;
                transition->this_state = state->state_type;
                transition->clips = nullptr;
                transition->num_clips = xml_count(transition_xml, "CLIP");
                if (transition->num_clips != 0)
                {
                    transition->clips = (audiolib_music_clip_data**)cursor;
                    audiolib_music_clip_data** target = transition->clips;
                    cursor += transition->num_clips * sizeof(audiolib_music_clip_data*);
                    for (xml_element* clip_xml = xml_find(transition_xml, "CLIP"); clip_xml != nullptr; clip_xml = xml_find_next(transition_xml, clip_xml, "CLIP"))
                    {
                        *target++ = audiolib_music_find_clip_after_colon(score, clip_xml->text);
                    }
                }
            }
        }
        // EXIT_TRACKS: weighted exit clips "<weight>:<clip>" (the first element only).
        xml_element* exit = xml_find(state_xml, "EXIT_TRACKS");
        if (exit != nullptr)
        {
            state->exit.this_state = state->state_type;
            const int count = xml_count(exit, "WEIGHTED");
            if (count != 0)
            {
                state->exit.decisions = (audiolib_music_clip_decision_data*)cursor;
                audiolib_music_clip_decision_data* decision = state->exit.decisions;
                cursor += count * sizeof(audiolib_music_clip_decision_data);
                for (xml_element* weighted = xml_find(exit, "WEIGHTED"); weighted != nullptr; weighted = xml_find_next(exit, weighted, "WEIGHTED"), ++decision)
                {
                    decision->type = AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED;
                    decision->value = (short)atoi(weighted->text);
                    decision->count = 0;
                    decision->dest_clip = audiolib_music_find_clip_after_colon(score, weighted->text);
                }
                state->exit.num_decisions = count;
            }
        }
    }
    // The first state (silence without one).
    const char* first_state = xml_table_get_opt_string_ref(score_xml, "FIRST_STATE");
    if (first_state == nullptr)
    {
        score->default_first_state = AUDIOLIB_MUSIC_STATE_SILENCE;
    }
    else
    {
        score->default_first_state = audiolib_music_state_from_text(first_state);
        score->curr_state = score->default_first_state;
    }
    // The "from" states of the state transitions.
    for (xml_element* state_xml = xml_find(score_xml, "STATE"); state_xml != nullptr; state_xml = xml_find_next(score_xml, state_xml, "STATE"))
    {
        audiolib_music_state_data* state_data = audiolib_internal_music_find_state_ptr_by_name(score, xml_table_get_req_string_ref(state_xml, "NAME"));
        if (state_data == nullptr || state_data->num_transitions <= 0)
        {
            continue;
        }
        for (xml_element* transition_xml = xml_find(state_xml, "TRANSITION_CLIP"); transition_xml != nullptr;
             transition_xml = xml_find_next(state_xml, transition_xml, "TRANSITION_CLIP"))
        {
            // The clip transitions are complete; only the attribute is checked (the slot search is unused).
            if (xml_table_get_req_string_ref(transition_xml, "FROM_CLIP") == nullptr)
            {
                return AUDIOLIB_ERR_FILE_FORMAT;
            }
        }
        for (xml_element* transition_xml = xml_find(state_xml, "TRANSITION"); transition_xml != nullptr;
             transition_xml = xml_find_next(state_xml, transition_xml, "TRANSITION"))
        {
            // The next state transition without a "from" state. Original: none left writes past the list.
            audiolib_music_transition_data* transition = state_data->transitions;
            for (int i = state_data->num_transitions;
                 i != 0 && (transition->trans_type != AUDIOLIB_MUSIC_TRANSITION_TYPE_STATE || transition->from_state != AUDIOLIB_MUSIC_STATE_UNKNOWN);
                 --i, ++transition)
            {
            }
            const char* from = xml_table_get_req_string_ref(transition_xml, "FROM");
            if (from == nullptr)
            {
                return AUDIOLIB_ERR_FILE_FORMAT;
            }
            transition->from_state = audiolib_music_state_from_text(from);
        }
    }
    // Clip states, and the cue named like the clip (it replaces CUE_NAME's).
    clip = score->clips;
    for (xml_element* clip_xml = xml_find(score_xml, "CLIP"); clip_xml != nullptr; clip_xml = xml_find_next(score_xml, clip_xml, "CLIP"), ++clip)
    {
        const char* state_name = xml_table_get_req_string_ref(clip_xml, "STATE");
        if (state_name == nullptr)
        {
            return AUDIOLIB_ERR_FILE_FORMAT;
        }
        const char* name = xml_table_get_req_string_ref(clip_xml, "NAME");
        clip->my_state = audiolib_internal_music_convert_state_name_to_enum(state_name, name);
        if (name != nullptr)
        {
            const unsigned int crc = crc_str(name, 0);
            if (Audiolib_state.initialized)
            {
                const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(crc, nullptr));
                if (foley != nullptr)
                {
                    clip->cue_id = (int)((unsigned int)(foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)foley << 16);
                }
            }
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00488940 (:1259), music_file in ESI
static bool audiolib_music_transition_state(audiolib_music_file_data* music_file)
{
    if (music_file == nullptr || music_file->score == nullptr)
    {
        return false;
    }
    Audiolib_music_cs.enter();
    music_file->score->curr_state = music_file->score->curr_clip->my_state;
    Audiolib_music_cs.leave();
    return true;
}

// 0x00495600 (:1280), music_file in EAX
static bool audiolib_music_clear_next_clip(audiolib_music_file_data* music_file)
{
    if (music_file == nullptr || music_file->score == nullptr)
    {
        return false;
    }
    Audiolib_music_cs.enter();
    audiolib_music_score_data* score = music_file->score;
    if (score->next_clip != nullptr)
    {
        score->next_clip = nullptr;
        if (score->next_play_id != -1)
        {
            audiolib_internal_playlist_fade_out(score->next_play_id, 0);
            score->next_play_id = -1;
        }
    }
    Audiolib_music_cs.leave();
    return true;
}

// The volume a clip plays at: music volume * cue volume * FSM scalar (double chain).
static float audiolib_music_clip_volume(float cue_volume)
{
    return (float)((double)Audiolib_Music.volume * (double)cue_volume * (double)Audiolib_Music.fsm_scalar);
}

// 0x004A0990 (:1306), music_file in ESI, next_clip in EDI
static bool audiolib_music_set_next_clip(audiolib_music_file_data* music_file, audiolib_music_clip_data* next_clip, bool start_paused)
{
    if (music_file == nullptr || music_file->score == nullptr || next_clip == nullptr)
    {
        return false;
    }
    Audiolib_music_cs.enter();
    audiolib_music_score_data* score = music_file->score;
    // Nothing playing: the clip starts at once.
    if (score->curr_clip == nullptr)
    {
        start_paused = false;
    }
    audiolib_music_clear_next_clip(music_file);
    audiolib_cue_info info;
    audiolib_cue_get_info(next_clip->cue_id, &info);
    // Params flag 4 (2: start paused); callback flags 0x1D4 (playing, stopped, destroyed, markers).
    audiolib_params params;
    params.flags = start_paused ? 6 : 4;
    params.volume = audiolib_music_clip_volume(info.volume);
    params.callback_flags = 0x1D4;
    params.callback_func = audiolib_interactive_music_callback;
    params.callback_data = music_file;
    // Into ambience: a 3.5 s fade in.
    if ((next_clip->my_state & 0xF) == 2)
    {
        params.fade_ms = 3500;
    }
    score->next_play_id = audiolib_cue_play(next_clip->cue_id, &params, nullptr);
    score->next_clip = next_clip;
    Audiolib_music_cs.leave();
    return true;
}

// 0x004A0AD0 (:1363)
static bool audiolib_music_cue_next_clip(audiolib_music_file_data* music_file, audiolib_music_clip_data* this_clip)
{
    Audiolib_music_cs.enter();
    audiolib_music_score_data* score = music_file != nullptr ? music_file->score : nullptr;
    if (score == nullptr || score->curr_clip == nullptr || this_clip == nullptr)
    {
        Audiolib_music_cs.leave();
        return false;
    }
    // A lull decides by its parent clip.
    audiolib_music_clip_data* parent = score->curr_clip->parent_clip;
    audiolib_music_clip_data* clip = parent != nullptr ? parent : score->curr_clip;
    audiolib_music_clip_decision_data* decisions = clip->decisions;
    audiolib_music_clip_data* chosen = nullptr;
    bool decide = true;
    if (decisions == nullptr)
    {
        // No decisions: a state transition of the clip's target state from its source state.
        const int from = this_clip->my_state & 0xF0;
        const int target = (this_clip->my_state & 0xF) << 4;
        audiolib_music_state_data* state = audiolib_internal_music_get_state_ptr_from_type(score, audiolib_music_state(target));
        if (state == nullptr)
        {
            Audiolib_music_cs.leave();
            return false;
        }
        audiolib_music_transition_data* transition = state->transitions;
        for (int i = state->num_transitions; i > 0; --i, ++transition)
        {
            if (transition->trans_type == AUDIOLIB_MUSIC_TRANSITION_TYPE_STATE && (transition->from_state & 0xF0) == from)
            {
                chosen = transition->clips[vrand() % (unsigned int)transition->num_clips];
                if (chosen != nullptr)
                {
                    chosen->my_state = audiolib_music_state(target >> 4 | from);
                    decide = false;
                }
                break;
            }
        }
    }
    else if (decisions->type == AUDIOLIB_MUSIC_CUE_DECISION_TYPE_LOOP)
    {
        // A loop decision first: the clip repeats a random number of times (min..max), then the weights decide.
        if (decisions->count == 0)
        {
            if (decisions->max <= decisions->value)
            {
                decisions->count = decisions->value;
            }
            else
            {
                decisions->count = (short)(vrand() % (unsigned int)(decisions->max - decisions->value)) + decisions->value;
            }
        }
        else if (decisions->count <= (short)clip->loop_count)
        {
            decisions->count = 0;
        }
        else
        {
            ++decisions->count;
            chosen = clip;
            decide = false;
        }
    }
    if (decide)
    {
        if (clip->num_decisions != 0)
        {
            // The weights shrink with use: (weight - uses) / total uses, the rest to the last decision.
            int total_uses = 0;
            for (int i = 0; i < clip->num_decisions; ++i)
            {
                audiolib_music_clip_decision_data& decision = clip->decisions[i];
                if (decision.type == AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED)
                {
                    total_uses += decision.count;
                    decision.max = decision.value;
                }
            }
            if (total_uses > 0)
            {
                int total = 0;
                for (int i = 0; i < clip->num_decisions; ++i)
                {
                    audiolib_music_clip_decision_data& decision = clip->decisions[i];
                    if (decision.type == AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED)
                    {
                        decision.max = (int)(((double)decision.value - (double)decision.count) / (double)(float)total_uses);
                        total += decision.max;
                    }
                }
                if (total < (int)clip->total_weights && clip->num_decisions != 0)
                {
                    clip->decisions[clip->num_decisions - 1].max += clip->total_weights - total;
                }
            }
            unsigned int roll = 0;
            if (clip->total_weights != 0)
            {
                roll = vrand() % (unsigned int)clip->total_weights;
            }
            // Up to 5 rolls avoid repeating the last decision. Original: the running sum is not reset between rolls.
            int sum = 0;
            for (int tries = 5; tries > 0;)
            {
                --tries;
                bool repeated = false;
                for (unsigned int i = 0; i < clip->num_decisions; i = (unsigned char)(i + 1))
                {
                    audiolib_music_clip_decision_data& decision = clip->decisions[i];
                    if (decision.type == AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED && (sum += decision.max, (int)roll < sum))
                    {
                        if ((int)i == clip->last_decision)
                        {
                            clip->last_decision = -1;
                            repeated = true;
                            break;
                        }
                        chosen = decision.dest_clip;
                        ++decision.count;
                        clip->last_decision = (signed char)i;
                        break;
                    }
                }
                if (!repeated && clip->last_decision != -1)
                {
                    break;
                }
            }
            if (clip->last_decision == -1)
            {
                chosen = clip;
            }
            // Every weight used up: the counts restart.
            if ((int)clip->total_weights <= total_uses + 1)
            {
                for (int i = 0; i < clip->num_decisions; ++i)
                {
                    if (clip->decisions[i].type == AUDIOLIB_MUSIC_CUE_DECISION_TYPE_WEIGHTED)
                    {
                        clip->decisions[i].count = 0;
                    }
                }
            }
            if (chosen == nullptr)
            {
                chosen = clip->decisions[0].dest_clip;
                ++clip->decisions[0].count;
            }
        }
        if (chosen == nullptr)
        {
            Audiolib_music_cs.leave();
            return false;
        }
    }
    // A lull continues with a lull of the chosen clip.
    audiolib_music_clip_data* next = chosen;
    if (parent != nullptr && chosen->num_lulls != 0)
    {
        next = chosen->lull_clips[vrand() % (unsigned int)chosen->num_lulls];
        if (next == nullptr)
        {
            Audiolib_music_cs.leave();
            return false;
        }
        next->parent_clip = chosen;
    }
    if (next == nullptr)
    {
        Audiolib_music_cs.leave();
        return false;
    }
    if (next != clip)
    {
        next->loop_count = 0;
    }
    audiolib_music_set_next_clip(music_file, next, true);
    Audiolib_music_cs.leave();
    return true;
}

// 0x00495660 (:1577), score in ESI
static bool audiolib_music_remove_from_old_list(audiolib_music_score_data* score, int play_id, bool kill, int fade_time)
{
    if (score == nullptr)
    {
        return false;
    }
    Audiolib_music_cs.enter();
    if (score->curr_clip != nullptr && play_id == score->curr_play_id)
    {
        Audiolib_music_cs.leave();
        return false;
    }
    if (!audiolib_internal_cue_is_fading(play_id))
    {
        audiolib_internal_playlist_fade_out(play_id, fade_time);
    }
    // A killed slot is marked -1 and cleared on the next pass.
    bool found = false;
    Audiolib_music_old_clip_list_cs.enter();
    for (int& old : score->old_clips)
    {
        if (old == 0)
        {
            continue;
        }
        if (old == -1)
        {
            old = 0;
            found = true;
        }
        else if (old == play_id && kill)
        {
            old = -1;
            found = true;
        }
    }
    Audiolib_music_old_clip_list_cs.leave();
    Audiolib_music_cs.leave();
    return found;
}

// 0x00488980 (:1622), score in ESI, clip_play_id in EDI
static bool audiolib_music_add_to_old_list(audiolib_music_score_data* score, int clip_play_id)
{
    if (score == nullptr || clip_play_id == 0)
    {
        return false;
    }
    if (clip_play_id == -1)
    {
        return true;
    }
    Audiolib_music_old_clip_list_cs.enter();
    for (int& old : score->old_clips)
    {
        if (old == 0)
        {
            old = clip_play_id;
            Audiolib_music_old_clip_list_cs.leave();
            return true;
        }
    }
    Audiolib_music_old_clip_list_cs.leave();
    return false;
}

// 0x00495760 (:1653)
static void audiolib_music_stop_all_old(audiolib_music_score_data* score, int fade_time)
{
    Audiolib_music_old_clip_list_cs.enter();
    for (int i = 0; i < 4; ++i)
    {
        const int play_id = score->old_clips[i];
        if (play_id == 0 || play_id == -1 || score == nullptr)
        {
            continue;
        }
        // audiolib_music_remove_from_old_list(score, play_id, true, fade_time), inlined.
        Audiolib_music_cs.enter();
        if (score->curr_clip == nullptr || play_id != score->curr_play_id)
        {
            if (!audiolib_internal_cue_is_fading(play_id))
            {
                audiolib_internal_playlist_fade_out(play_id, fade_time);
            }
            Audiolib_music_old_clip_list_cs.enter();
            for (int& old : score->old_clips)
            {
                if (old == 0)
                {
                    continue;
                }
                if (old == -1)
                {
                    old = 0;
                }
                else if (old == play_id)
                {
                    old = -1;
                }
            }
            Audiolib_music_old_clip_list_cs.leave();
        }
        Audiolib_music_cs.leave();
    }
    Audiolib_music_old_clip_list_cs.leave();
}

// The next clip becomes the current one.
static void audiolib_music_promote_next(audiolib_music_score_data* score)
{
    audiolib_music_clip_data* next = score->next_clip;
    score->curr_clip = next;
    score->curr_play_id = score->next_play_id;
    score->next_clip = nullptr;
    score->next_play_id = -1;
    if (next != nullptr)
    {
        score->curr_state = next->my_state;
    }
}

// 0x004A0E40 (:1674)
void audiolib_interactive_music_callback(AUDIOLIB_CUE_CALLBACK_DATA* cue_cb_data)
{
    Audiolib_music_cs.enter();
    audiolib_music_file_data* music_file = (audiolib_music_file_data*)cue_cb_data->user_data;
    if (music_file == nullptr || music_file->score == nullptr || (music_file->action & 0x10) != 0)
    {
        Audiolib_music_cs.leave();
        return;
    }
    audiolib_music_score_data* score = music_file->score;
    audiolib_music_clip_data* next = score->next_clip;
    const int next_play_id = score->next_play_id;
    audiolib_music_clip_data* curr = score->curr_clip;
    const int play_id = cue_cb_data->play_id;
    audiolib_music_clip_data* this_clip;
    if (curr != nullptr && score->curr_play_id == play_id)
    {
        this_clip = curr;
    }
    else if (next != nullptr && next_play_id == play_id)
    {
        this_clip = next;
    }
    else
    {
        // An old clip: faded out (500 ms) and forgotten.
        audiolib_music_remove_from_old_list(score, play_id, false, 500);
        Audiolib_music_cs.leave();
        return;
    }
    switch (cue_cb_data->action)
    {
    case 0x10:
        // Playing: the next clip became the current one, and the one after it is cued.
        if (!Audiolib_Music.paused)
        {
            if (this_clip == next)
            {
                if (curr != nullptr && curr != next)
                {
                    audiolib_music_add_to_old_list(score, score->curr_play_id);
                    score->curr_clip = nullptr;
                    score->curr_play_id = -1;
                }
                score->curr_clip = score->next_clip;
                score->curr_play_id = score->next_play_id;
                score->next_clip = nullptr;
                audiolib_music_transition_state(music_file);
                audiolib_music_cue_next_clip(music_file, this_clip);
            }
            else if (this_clip == curr)
            {
                audiolib_music_transition_state(music_file);
                if (next == nullptr)
                {
                    audiolib_music_cue_next_clip(music_file, this_clip);
                }
            }
        }
        break;
    case 0x40:
    {
        // Markers: 'E' end (switch now), 'F' fade (switch early into a higher state), 'T' transition.
        const int marker = toupper(cue_cb_data->marker_text[0]);
        if (marker != 'E')
        {
            if (marker == 'F')
            {
                if (!this_clip->transition_at_next_marker)
                {
                    if (this_clip == curr && score->next_clip != nullptr && (score->curr_clip->my_state & 0xF) * 0x10 < (score->next_clip->my_state & 0xF) * 0x10)
                    {
                        // Rising: the next clip starts, the current one fades over 3.5 s.
                        audiolib_play_set_pause(score->next_play_id, false);
                        if (Audiolib_state.initialized && play_id != 0 && play_id != -1)
                        {
                            audiolib_internal_playlist_fade_out(play_id, 3500);
                        }
                        audiolib_music_promote_next(score);
                    }
                    break;
                }
                this_clip->transition_at_next_marker = false;
            }
            else if (marker != 'T')
            {
                break;
            }
            // Not into a lower state (ambience excepted).
            if (this_clip == curr && score->next_clip != nullptr)
            {
                const int next_target = score->next_clip->my_state & 0xF;
                if (next_target != 2 && next_target * 0x10 <= (score->curr_clip->my_state & 0xF) * 0x10)
                {
                    break;
                }
            }
            if (Audiolib_state.initialized && play_id != 0 && play_id != -1)
            {
                audiolib_internal_playlist_fade_out(play_id, 3500);
            }
        }
        if (next != nullptr && this_clip == curr && !audiolib_is_playing(next_play_id, false))
        {
            audiolib_play_set_pause(next_play_id, false);
        }
        if (score->curr_clip != nullptr)
        {
            audiolib_music_add_to_old_list(score, score->curr_play_id);
            score->curr_clip = nullptr;
            score->curr_play_id = -1;
        }
        audiolib_music_promote_next(score);
        break;
    }
    case 0x80:
    case 0x100:
        // Stopped or destroyed.
        if (curr == nullptr || play_id != score->curr_play_id)
        {
            if (next != nullptr && play_id == next_play_id)
            {
                if (score->curr_state == AUDIOLIB_MUSIC_STATE_SILENCE || (curr != nullptr && score->curr_play_id != -1))
                {
                    score->next_clip = nullptr;
                    score->next_play_id = -1;
                }
                else
                {
                    Audiolib_Reboot_Music_System = true;
                }
            }
        }
        else
        {
            if (next == nullptr)
            {
                score->curr_clip = nullptr;
                score->curr_play_id = -1;
            }
            else
            {
                if (!audiolib_is_playing(next_play_id, false))
                {
                    audiolib_play_set_pause(next_play_id, false);
                }
                audiolib_music_promote_next(score);
            }
            audiolib_music_add_to_old_list(score, play_id);
        }
        break;
    default:
        break;
    }
    // Nothing left outside silence: the game restarts the music.
    if (score->curr_clip == nullptr && score->next_clip == nullptr && score->curr_state != AUDIOLIB_MUSIC_STATE_SILENCE)
    {
        Audiolib_Reboot_Music_System = true;
    }
    Audiolib_music_cs.leave();
}

// 0x004889D0 (:1999)
int audiolib_internal_music_get_first_clip_index(audiolib_music_score_data* score, AUDIOLIB_MUSIC_STATE state_type)
{
    audiolib_music_state_data* state = audiolib_internal_music_get_state_ptr_from_type(score, state_type);
    if (state == nullptr)
    {
        return 0;
    }
    // A random first clip not played yet (all played: the counts restart).
    unsigned int index = 0;
    if (state->num_first_clips != 0)
    {
        index = vrand() % (unsigned int)state->num_first_clips;
        int tries = 0;
        while (state->first_clip_usage_counts[index] != 0)
        {
            ++tries;
            if (tries >= state->num_first_clips)
            {
                memset(state->first_clip_usage_counts, 0, state->num_first_clips * 4);
            }
            index = vrand() % (unsigned int)state->num_first_clips;
        }
        ++state->first_clip_usage_counts[index];
    }
    return (int)index;
}

// 0x0049B840 (:2029)
void audiolib_internal_process_music_stop(audiolib_music_file_data* music)
{
    if (music == nullptr)
    {
        return;
    }
    Music_list.move(music, AUDIOLIB_MUSIC_LOAD_STATE_STOPPING);
    // The stop's fade time, 500 ms by default.
    int fade_time = 500;
    if (Audiolib_Music_Fade_Time != 0)
    {
        fade_time = Audiolib_Music_Fade_Time;
        Audiolib_Music_Fade_Time = 0;
    }
    audiolib_music_score_data* score = music->score;
    if (score != nullptr)
    {
        if (score->next_clip != nullptr)
        {
            audiolib_internal_playlist_fade_out(score->next_play_id, fade_time);
            music->score->next_play_id = -1;
        }
        music->score->next_clip = nullptr;
        if (music->score->curr_clip != nullptr)
        {
            audiolib_internal_playlist_fade_out(music->score->curr_play_id, fade_time);
            music->score->curr_play_id = -1;
        }
        music->score->curr_clip = nullptr;
        music->score->curr_play_id = -1;
        audiolib_music_stop_all_old(music->score, fade_time);
        music->score->curr_state = AUDIOLIB_MUSIC_STATE_UNKNOWN;
        music->action = 0;
    }
    Music_list.move(music, AUDIOLIB_MUSIC_LOAD_STATE_READY);
    if (Audiolib_Music.current_music_id == music->music_id)
    {
        Audiolib_Music.current_music_id = -1;
    }
}

// A clip play's volume follows the music volume (process_music's per-clip update).
static void audiolib_music_update_clip_volume(int play_id, const audiolib_music_clip_data* clip)
{
    // Original: the read volume and the cue volume are left uninitialized when unavailable; here they are 0.
    float volume = 0.0f;
    if (Audiolib_state.initialized && play_id != 0 && play_id != -1)
    {
        audiolib_internal_playlist_get_volume(play_id, &volume);
    }
    audiolib_cue_info info;
    memset(&info, 0, sizeof(info));
    // audiolib_cue_get_info, inlined.
    if (Audiolib_state.initialized && clip->cue_id != -1 && audiolib_get_foley_data_pointer(clip->cue_id) != nullptr)
    {
        audiolib_cue_get_info(clip->cue_id, &info);
    }
    const float target = audiolib_music_clip_volume(info.volume);
    if (volume != target && Audiolib_state.initialized && play_id != 0 && play_id != -1)
    {
        audiolib_internal_playlist_set_volume(play_id, target, 0);
    }
}

// 0x0049B920 (:2076)
bool audiolib_process_music(void* user_data, int time, int time_step)
{
    (void)user_data;
    (void)time_step;
    // The music volume moves to the fade volume (or the destination volume) by its distance over the 100 ms steps
    // left, landing within 0.001 (double chains as rfg.exe).
    const float target = Audiolib_Music.fade_volume == 0.0f ? Audiolib_Music.dest_volume : Audiolib_Music.fade_volume;
    if ((double)Audiolib_Music.volume != (double)target)
    {
        bool land = true;
        if (Audiolib_Music.fade_time > 0 && time < Audiolib_Music.fade_time)
        {
            const int steps = (Audiolib_Music.fade_time - time) / 100;
            const float distance = (float)fabs((double)(float)((double)Audiolib_Music.volume - (double)target));
            const float step = (float)((double)distance / (double)(float)steps);
            if ((double)Audiolib_Music.volume > (double)target)
            {
                Audiolib_Music.volume = (float)((double)Audiolib_Music.volume - (double)step);
            }
            else
            {
                Audiolib_Music.volume = (float)((double)Audiolib_Music.volume + (double)step);
            }
            land = (float)fabs((double)(float)((double)Audiolib_Music.volume - (double)target)) < 0.001f;
        }
        if (land)
        {
            Audiolib_Music.fade_time = 0;
            Audiolib_Music.volume = target;
        }
    }
    Audiolib_music_cs.enter();
    // Ready music: stops, or starts (the cued first clip unpaused).
    Music_list.thread_access.enter();
    audiolib_music_file_data* next = Music_list.get_first(AUDIOLIB_MUSIC_LOAD_STATE_READY);
    while (next != nullptr)
    {
        audiolib_music_file_data* music = next;
        next = Music_list.get_next(music);
        const unsigned short action = music->action;
        if ((action & 0x10) != 0)
        {
            audiolib_internal_process_music_stop(music);
            music->action &= 0xFFED;
            continue;
        }
        if ((action & 2) == 0)
        {
            continue;
        }
        music->action = action & 0xFFED;
        audiolib_music_score_data* score = music->score;
        if (score == nullptr)
        {
            continue;
        }
        const int next_play_id = score->next_play_id;
        if (score->next_clip != nullptr && Audiolib_state.initialized && next_play_id != 0 && next_play_id != -1)
        {
            // audiolib_internal_playlist_set_pause(next_play_id, false), inlined.
            Audiolib_play_cs.enter();
            audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(next_play_id);
            if (play == nullptr)
            {
                Audiolib_play_cs.leave();
            }
            else
            {
                unsigned short play_action = play->action;
                if ((play_action & 0x10) == 0)
                {
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
                            play->action = play_action | 2;
                        }
                        break;
                    case AUDIOLIB_HW_PLAY_STATE_READY:
                        play->params.flags &= 0xFFF9;
                        play->action = (play_action & 0xFFFB) | 10;
                        break;
                    case AUDIOLIB_HW_PLAY_STATE_PAUSED:
                        play->action = (play_action & 0xFFFB) | 8;
                        break;
                    default:
                        break;
                    }
                }
                Audiolib_play_cs.leave();
                music->score->curr_clip = music->score->next_clip;
                music->score->curr_play_id = music->score->next_play_id;
                music->score->next_clip = nullptr;
                music->score->next_play_id = -1;
            }
        }
        Music_list.move(music, AUDIOLIB_MUSIC_LOAD_STATE_PLAYING);
        Audiolib_Music.current_music_id = music->music_id;
    }
    Music_list.thread_access.leave();
    // Playing music: stops, or its clips follow the music volume.
    Music_list.thread_access.enter();
    next = Music_list.get_first(AUDIOLIB_MUSIC_LOAD_STATE_PLAYING);
    while (next != nullptr)
    {
        audiolib_music_file_data* music = next;
        next = Music_list.get_next(music);
        if ((music->action & 0x10) != 0)
        {
            audiolib_internal_process_music_stop(music);
            music->action &= 0xFFED;
            continue;
        }
        audiolib_music_score_data* score = music->score;
        if (score == nullptr)
        {
            continue;
        }
        if (score->curr_clip != nullptr)
        {
            audiolib_music_update_clip_volume(score->curr_play_id, music->score->curr_clip);
        }
        if (music->score->next_clip != nullptr)
        {
            audiolib_music_update_clip_volume(music->score->next_play_id, music->score->next_clip);
        }
    }
    Music_list.thread_access.leave();
    Audiolib_music_cs.leave();
    return false;
}

// 0x004A5EC0 (:2225)
bool audiolib_interactive_music_system_init()
{
    if (Audiolib_Music.inited)
    {
        return true;
    }
    Audiolib_Music.current_music_id = -1;
    // audiolib_thread_function_register, inlined.
    audiolib_process_data* process = Audio_process_list.alloc(0);
    if (process == nullptr)
    {
        return false;
    }
    process->user_data = nullptr;
    process->function = audiolib_process_music;
    strncpy(process->func_name, "audiolib_music_process", sizeof(process->func_name));
    Audiolib_Music.volume = 1.0f;
    Audiolib_Music.dest_volume = 1.0f;
    Audiolib_Music.fade_volume = 0.0f;
    Audiolib_Music.fade_time = 0;
    Audiolib_Music.fsm_scalar = 1.0f;
    Audiolib_Music.play_exit_cue = false;
    // rfg_music.xtbl names up to 20 music tables ("<NAME>.mtbl"), all loaded now.
    char names[20][64];
    int count = 0;
    xml_element* root = xml_table_parse("rfg_music.xtbl");
    xml_element* music = xml_find(root, "MUSIC");
    if (music != nullptr)
    {
        do
        {
            strncpy(names[count], xml_table_get_req_string_ref(music, "NAME"), 64);
            strcat(names[count], ".mtbl");
            ++count;
        } while (count < 20 && (music = xml_find_next(root, music, "MUSIC")) != nullptr);
    }
    xml_table_close(root);
    for (int i = 0; i < count; ++i)
    {
        audiolib_interactive_music_load_table(names[i], nullptr);
    }
    Audiolib_Music.inited = true;
    return true;
}

// 0x004A6050 (:2271)
void audiolib_interactive_music_system_shutdown()
{
    if (Audiolib_Music.inited)
    {
        audiolib_interactive_music_stop(Audiolib_Music.current_music_id, 0);
        audiolib_thread_function_unregister(audiolib_process_music);
        Audiolib_Music.inited = false;
    }
}

// 0x004A6080 (:2290)
audiolib_result audiolib_interactive_music_get_id(const char* music_file_name, int* music_id)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    return audiolib_interactive_music_load_table(music_file_name, music_id);
}

// 0x00488A50 (:2300)
int audiolib_interactive_music_get_active_id()
{
    return Audiolib_Music.inited ? Audiolib_Music.current_music_id : -1;
}

// 0x004A1320 (:2310)
audiolib_result audiolib_interactive_music_load_table(const char* music_file_name, int* music_id)
{
    if (music_file_name == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_result result = AUDIOLIB_ERR_NONE;
    Audiolib_music_cs.enter();
    audiolib_music_file_data* file = audiolib_internal_find_music_table(music_file_name);
    if (file != nullptr)
    {
        if (music_id != nullptr)
        {
            *music_id = file->music_id;
        }
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    file = Music_list.alloc(AUDIOLIB_MUSIC_LOAD_STATE_WAIT_FOR_LOAD);
    if (file == nullptr)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NO_MORE_MUSIC_SLOTS;
    }
    strncpy(file->name, music_file_name, sizeof(file->name));
    file->name_crc = crc_str(music_file_name, 0);
    file->music_id = (Music_list.get_index(file) & 0xFFFF) | (file->rolling_counter << 16);
    if (music_id != nullptr)
    {
        *music_id = file->music_id;
    }
    Music_list.move(file, AUDIOLIB_MUSIC_LOAD_STATE_WAIT_FOR_LOAD);
    xml_element* root = xml_table_parse(file->name);
    Music_list.move(file, AUDIOLIB_MUSIC_LOAD_STATE_PARSING);
    if (root != nullptr)
    {
        xml_element* music = xml_table_find_opt(root, "MUSIC");
        if (music != nullptr)
        {
            result = audiolib_internal_parse_music_table(file, music);
        }
    }
    xml_table_close(root);
    if (result < 0)
    {
        // Original: a bad table is dropped silently (and its id was already given out).
        audiolib_interactive_music_unload_table(file->music_id);
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    Music_list.move(file, AUDIOLIB_MUSIC_LOAD_STATE_READY);
    Audiolib_music_cs.leave();
    return result;
}

// 0x0049BFB0 (:2372)
audiolib_result audiolib_interactive_music_unload_table(int music_id)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    audiolib_music_file_data* file = audiolib_internal_music_id_to_ptr(music_id);
    if (file == nullptr)
    {
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    // The score stays in the audio pool.
    Audiolib_music_cs.enter();
    Music_list.move(file, AUDIOLIB_MUSIC_LOAD_STATE_UNLOADING);
    Music_list.free(file);
    Audiolib_music_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00488A70 (:2402)
audiolib_result audiolib_interactive_music_get_volume(float* volume)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (volume == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *volume = Audiolib_Music.volume;
    return AUDIOLIB_ERR_NONE;
}

// 0x00488AA0 (:2412)
audiolib_result audiolib_interactive_music_set_volume(const float volume, int fade_time_ms)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_Music.dest_volume = volume;
    Audiolib_Music.fade_time = timer_get(TIMER_MILLISECONDS) + fade_time_ms;
    return AUDIOLIB_ERR_NONE;
}

// 0x00488AE0 (:2422)
audiolib_result audiolib_interactive_music_get_fade(bool* fade, float* volume)
{
    const float fade_volume = Audiolib_Music.fade_volume;
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (fade == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    *fade = false;
    if (fade_volume != 0.0f)
    {
        *fade = true;
        if (volume != nullptr)
        {
            *volume = fade_volume;
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00488B30 (:2437)
audiolib_result audiolib_interactive_music_set_fade(const float volume, int fade_time_ms)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_Music.fade_volume = volume;
    Audiolib_Music.fade_time = timer_get(TIMER_MILLISECONDS) + fade_time_ms;
    return AUDIOLIB_ERR_NONE;
}

// 0x00488B70 (:2447)
audiolib_result audiolib_interactive_music_stop_fade(int fade_time_ms)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    Audiolib_Music.fade_volume = 0.0f;
    Audiolib_Music.fade_time = timer_get(TIMER_MILLISECONDS) + fade_time_ms;
    return AUDIOLIB_ERR_NONE;
}

// 0x004A60A0 (:2461)
audiolib_result audiolib_interactive_music_start(int music_id, AUDIOLIB_MUSIC_STATE start_state, const float volume)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (Audiolib_Music.current_music_id == music_id)
    {
        return AUDIOLIB_ERR_NONE;
    }
    // A new volume fades in over 2 s.
    if (0.0f < volume)
    {
        Audiolib_Music.volume = volume;
        Audiolib_Music.dest_volume = volume;
        Audiolib_Music.fade_time = timer_get(TIMER_MILLISECONDS) + 2000;
    }
    if (Audiolib_Music.current_music_id != -1)
    {
        audiolib_interactive_music_stop(Audiolib_Music.current_music_id, 0);
        Audiolib_Music.current_music_id = -1;
    }
    audiolib_music_file_data* file = audiolib_internal_music_id_to_ptr(music_id);
    if (file == nullptr || file->score == nullptr)
    {
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    audiolib_music_score_data* score = file->score;
    // The given state, the score's first state, or its first state entry.
    audiolib_music_state_data* state = nullptr;
    if (start_state != AUDIOLIB_MUSIC_STATE_UNKNOWN)
    {
        state = audiolib_internal_music_get_state_ptr_from_type(score, start_state);
    }
    if (state == nullptr)
    {
        state = audiolib_internal_music_get_state_ptr_from_type(score, score->default_first_state);
    }
    if (state == nullptr)
    {
        state = score->state_data;
    }
    if (state != nullptr && state->first_clips == nullptr && state->state_type != AUDIOLIB_MUSIC_STATE_SILENCE)
    {
        Audiolib_Music.current_music_id = -1;
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    score->curr_state = state->state_type;
    score = file->score;
    // Silence starts without a clip.
    if (score->curr_state < 0x12)
    {
        score->next_clip = nullptr;
        file->action |= 2;
        return AUDIOLIB_ERR_NONE;
    }
    const int index = audiolib_internal_music_get_first_clip_index(score, score->curr_state);
    state = audiolib_internal_music_get_state_ptr_from_type(file->score, file->score->curr_state);
    if (state == nullptr)
    {
        return AUDIOLIB_ERR_NONE;
    }
    audiolib_music_clip_data* clip = state->first_clips[index];
    if (!audiolib_cue_is_id_valid(clip->cue_id))
    {
        return AUDIOLIB_ERR_BAD_CUE_ID;
    }
    // The first clip is cued paused, 2 s ahead; process_music starts it.
    audiolib_cue_info info;
    audiolib_cue_get_info(clip->cue_id, &info);
    audiolib_params params;
    params.volume = audiolib_music_clip_volume(info.volume);
    params.flags = 6;
    params.callback_flags = 0x1D4;
    params.callback_func = audiolib_interactive_music_callback;
    params.delay_ms = 2000;
    params.callback_data = file;
    const int play_id = audiolib_cue_play(clip->cue_id, &params, nullptr);
    if (play_id == -1)
    {
        return AUDIOLIB_ERR_NONE;
    }
    file->score->next_clip = clip;
    file->score->next_play_id = play_id;
    Audiolib_Music.current_music_id = file->music_id;
    file->action |= 2;
    return AUDIOLIB_ERR_NONE;
}

// 0x00495830 (:2599)
AUDIOLIB_MUSIC_STATE audiolib_interactive_music_get_state(int music_id)
{
    if (!Audiolib_Music.inited || Audiolib_Music.current_music_id == -1 || Audiolib_Music.current_music_id != music_id)
    {
        return AUDIOLIB_MUSIC_STATE_UNKNOWN;
    }
    AUDIOLIB_MUSIC_STATE state = AUDIOLIB_MUSIC_STATE_UNKNOWN;
    Audiolib_music_cs.enter();
    const audiolib_music_file_data* file = audiolib_internal_music_id_to_ptr(Audiolib_Music.current_music_id);
    if (file != nullptr && file->score != nullptr)
    {
        state = file->score->curr_state;
    }
    Audiolib_music_cs.leave();
    return state;
}

// 0x004A1470 (:2628)
audiolib_result audiolib_interactive_music_transition(bool reboot, int music_id, AUDIOLIB_MUSIC_STATE new_state, const float volume, bool asap)
{
    (void)reboot;
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (Audiolib_Music.current_music_id == -1 || Audiolib_Music.current_music_id != music_id)
    {
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    bool cued = false;
    Audiolib_music_cs.enter();
    audiolib_music_file_data* music_file = audiolib_internal_music_id_to_ptr(Audiolib_Music.current_music_id);
    if (music_file == nullptr || music_file->score == nullptr)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    audiolib_music_score_data* score = music_file->score;
    // Already there (or cued there).
    if (new_state == score->curr_state && new_state < 0x66 && (score->next_clip == nullptr || score->next_clip->my_state == new_state))
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    const int new_home = new_state & 0xF0;
    int state_type = new_home;
    bool to_lull = false;
    if (new_home == 0x70)
    {
        state_type = 0x60;
        to_lull = true;
    }
    else if (new_home == 0x50)
    {
        state_type = 0x40;
        to_lull = true;
    }
    // The state the current one heads for (lulls as their combat state).
    const int curr_target = score->curr_state & 0xF;
    int curr_home = curr_target * 0x10;
    if (curr_target == 7)
    {
        curr_home = 0x60;
    }
    else if (curr_target == 5)
    {
        curr_home = 0x40;
    }
    audiolib_music_state_data* state = audiolib_internal_music_get_state_ptr_from_type(music_file->score, audiolib_music_state(state_type));
    if (state == nullptr || audiolib_internal_music_get_state_ptr_from_type(music_file->score, audiolib_music_state(curr_home)) == nullptr)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    if ((curr_home < 0x40 || state_type > 0x3F) && new_state != AUDIOLIB_MUSIC_STATE_SILENCE)
    {
        // Into music: a transition clip, or a first clip from silence (or with nothing playing).
        audiolib_music_clip_data* clip = audiolib_internal_music_find_transition_clip(music_file->score, audiolib_music_state(to_lull ? new_home : state_type));
        if (clip == nullptr)
        {
            if (music_file->score->curr_state != AUDIOLIB_MUSIC_STATE_SILENCE)
            {
                Audiolib_music_cs.leave();
                return AUDIOLIB_ERR_NONE;
            }
            const int index = audiolib_internal_music_get_first_clip_index(music_file->score, audiolib_music_state(state_type));
            cued = audiolib_music_set_next_clip(music_file, state->first_clips[index], true);
        }
        else if (music_file->score->curr_clip == nullptr)
        {
            const int index = audiolib_internal_music_get_first_clip_index(music_file->score, audiolib_music_state(state_type));
            cued = audiolib_music_set_next_clip(music_file, state->first_clips[index], true);
        }
        else
        {
            cued = audiolib_music_set_next_clip(music_file, clip, true);
        }
    }
    else
    {
        // Down from combat (or to silence): a transition clip, else the music fades out over 1 s into silence.
        audiolib_music_clip_data* clip = nullptr;
        if (state_type != 0x10)
        {
            clip = audiolib_internal_music_find_transition_clip(music_file->score, audiolib_music_state(to_lull ? new_home : state_type));
        }
        if (clip != nullptr)
        {
            cued = audiolib_music_set_next_clip(music_file, clip, true);
        }
        else
        {
            audiolib_music_clear_next_clip(music_file);
            const int play_id = music_file->score->curr_play_id;
            if (music_file->score->curr_clip != nullptr && Audiolib_state.initialized && play_id != 0 && play_id != -1)
            {
                audiolib_internal_playlist_fade_out(play_id, 1000);
            }
            music_file->score->curr_state = AUDIOLIB_MUSIC_STATE_SILENCE;
        }
    }
    // A new volume: 0.5 s from light to heavy combat, 2 s otherwise.
    if (0.0f <= volume)
    {
        Audiolib_Music.dest_volume = volume;
        Audiolib_Music.fade_time = timer_get(TIMER_MILLISECONDS) + (state_type == 0x60 && curr_home == 0x40 ? 500 : 2000);
    }
    score = music_file->score;
    if (score->curr_clip == nullptr)
    {
        // Nothing playing: the cued clip starts now.
        if (score->next_clip == nullptr || audiolib_play_get_state(score->next_play_id, nullptr) != AUDIOLIB_ERR_NONE)
        {
            Audiolib_music_cs.leave();
            return AUDIOLIB_ERR_MUSIC_TRANSITION_FAILED;
        }
        if (!audiolib_is_playing(music_file->score->next_play_id, false))
        {
            audiolib_play_set_pause(music_file->score->next_play_id, false);
        }
        music_file->score->curr_clip = music_file->score->next_clip;
        music_file->score->curr_play_id = music_file->score->next_play_id;
        music_file->score->next_clip = nullptr;
        music_file->score->next_play_id = -1;
        music_file->score->curr_state = audiolib_music_state(state_type >> 4 | curr_home);
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    if (!cued || score->next_clip == nullptr)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // The switch point: the current clip's next marker, or a bar line in time.
    int marker_time = -1;
    int start_time = -1;
    char* marker_text;
    const int now = timer_get(TIMER_MILLISECONDS);
    const audiolib_result marker = audiolib_marker_get_next_marker(music_file->score->curr_play_id, &marker_time, &start_time, &marker_text);
    bool at_marker = marker != AUDIOLIB_ERR_NONE;
    if (!at_marker && !asap)
    {
        // The marker within 3.5 s.
        const int remaining = start_time - now + marker_time;
        at_marker = remaining >= 0 && remaining <= 3500;
    }
    score = music_file->score;
    if (!at_marker && (score->curr_state & 0xF0) >= 0x40 && !Audiolib_Music_Level_Limited && new_state > 0x22)
    {
        at_marker = true;
    }
    if (at_marker)
    {
        if (music_file->score->curr_clip == nullptr || audiolib_play_get_state(music_file->score->curr_play_id, nullptr) != AUDIOLIB_ERR_NONE)
        {
            if (music_file->score->next_clip == nullptr || audiolib_play_get_state(music_file->score->next_play_id, nullptr) != AUDIOLIB_ERR_NONE)
            {
                Audiolib_Reboot_Music_System = true;
            }
            else if (!audiolib_is_playing(music_file->score->next_play_id, false))
            {
                audiolib_play_set_pause(music_file->score->next_play_id, false);
                Audiolib_music_cs.leave();
                return AUDIOLIB_ERR_NONE;
            }
        }
        else if (!audiolib_is_playing(music_file->score->curr_play_id, false) && music_file->score->next_clip != nullptr &&
                 audiolib_play_get_state(music_file->score->next_play_id, nullptr) == AUDIOLIB_ERR_NONE &&
                 !audiolib_is_playing(music_file->score->next_play_id, false))
        {
            audiolib_play_set_pause(music_file->score->next_play_id, false);
            Audiolib_music_cs.leave();
            return AUDIOLIB_ERR_NONE;
        }
        // Original: with no current clip left this writes through null.
        music_file->score->curr_clip->transition_at_next_marker = true;
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // The beat length from the tempo (571 ms when it rounds to 0; original: a zero tempo divides by zero).
    audiolib_music_clip_data* curr = score->curr_clip;
    int beat_ms = 60000 / (int)curr->tempo;
    if (beat_ms == 0)
    {
        beat_ms = 571;
    }
    if (now > start_time + marker_time)
    {
        // Past the marker: an end marker switches at the next one.
        if (toupper(*marker_text) == 'E' && music_file->score->next_clip != nullptr)
        {
            music_file->score->next_clip->transition_at_next_marker = true;
        }
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // The next bar line at least half a bar away gets an extra 'T' marker 25 ms early, if it comes a bar before the
    // marker.
    const int beat = (now - start_time) / beat_ms;
    const unsigned int bar = music_file->score->curr_clip->integral_beat;
    int next_bar = (int)(bar - beat % (int)bar) + beat;
    if ((float)(((double)(float)next_bar - (double)(float)beat) / (double)(int)bar) < 0.5f)
    {
        next_bar += curr->integral_beat;
    }
    if (marker_time / beat_ms - next_bar > (int)curr->integral_beat)
    {
        audiolib_play_add_extra_marker(score->curr_play_id, next_bar * beat_ms - 25, "T");
    }
    Audiolib_music_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004A1AA0 (:3089)
audiolib_result audiolib_interactive_music_stop(int music_id, int fade_time)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (Audiolib_Music.current_music_id != music_id)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_music_cs.enter();
    audiolib_music_file_data* file = audiolib_internal_music_id_to_ptr(Audiolib_Music.current_music_id);
    if (file == nullptr)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    audiolib_music_score_data* score = file->score;
    if (score == nullptr || score->curr_state == AUDIOLIB_MUSIC_STATE_UNKNOWN)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_BAD_INTERNAL;
    }
    const int category = Music_list.get_category(file);
    if (category == AUDIOLIB_MUSIC_LOAD_STATE_PLAYING)
    {
        // A requested exit cue plays once.
        if (Audiolib_Music.play_exit_cue)
        {
            audiolib_music_clip_data* clip = audiolib_internal_music_get_exit_cue(score);
            if (clip != nullptr && clip->cue_id != -1)
            {
                audiolib_cue_info info;
                audiolib_cue_get_info(clip->cue_id, &info);
                audiolib_params params;
                params.volume = audiolib_music_clip_volume(info.volume);
                params.flags = 4;
                params.callback_flags = 0x1D4;
                params.callback_func = audiolib_interactive_music_callback;
                params.callback_data = file;
                audiolib_cue_play(clip->cue_id, &params, nullptr);
            }
            Audiolib_Music.play_exit_cue = false;
        }
    }
    else if (category != AUDIOLIB_MUSIC_LOAD_STATE_PAUSED)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // process_music stops it.
    Audiolib_Music_Fade_Time = fade_time;
    file->action |= 0x10;
    Audiolib_music_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00488BB0 (:3156)
void audiolib_music_play_exit_cue()
{
    Audiolib_Music.play_exit_cue = true;
}

// 0x00488BC0 (:3162)
bool audiolib_interactive_music_get_pause(int music_id)
{
    return Audiolib_Music.inited && Audiolib_Music.current_music_id == music_id && Audiolib_Music.paused;
}

// 0x00488BF0 (:3174)
bool audiolib_interactive_music_get_reboot_status()
{
    return Audiolib_Reboot_Music_System;
}

// 0x00488C00 (:3179)
void audiolib_interactive_music_clear_reboot()
{
    Audiolib_Reboot_Music_System = false;
}

// 0x0049C030 (:3184)
audiolib_result audiolib_interactive_music_set_pause(bool pause)
{
    if (!Audiolib_Music.inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (Audiolib_Music.paused == pause)
    {
        return AUDIOLIB_ERR_NONE;
    }
    Audiolib_music_cs.enter();
    audiolib_music_file_data* file = audiolib_internal_music_id_to_ptr(Audiolib_Music.current_music_id);
    if (file == nullptr)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_BAD_MUSIC_ID;
    }
    if (file->score == nullptr || file->score->curr_state == AUDIOLIB_MUSIC_STATE_UNKNOWN)
    {
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_BAD_INTERNAL;
    }
    const int category = Music_list.get_category(file);
    if (pause && category == AUDIOLIB_MUSIC_LOAD_STATE_PLAYING)
    {
        Audiolib_music_cs.enter();
        Music_list.move(file, AUDIOLIB_MUSIC_LOAD_STATE_PAUSED);
        if (file->score->curr_clip != nullptr && file->score->curr_play_id != -1)
        {
            audiolib_play_set_pause(file->score->curr_play_id, true);
        }
        if (file->score->next_clip != nullptr && file->score->next_play_id != -1)
        {
            audiolib_play_set_pause(file->score->next_play_id, true);
        }
        Audiolib_Music.paused = true;
        Audiolib_music_cs.leave();
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    if (!pause && category == AUDIOLIB_MUSIC_LOAD_STATE_PAUSED)
    {
        // Only the current clip resumes (none: the game restarts the music).
        Audiolib_music_cs.enter();
        Music_list.move(file, AUDIOLIB_MUSIC_LOAD_STATE_PLAYING);
        if (file->score->curr_clip == nullptr || file->score->curr_play_id == -1)
        {
            Audiolib_Reboot_Music_System = true;
        }
        else
        {
            audiolib_play_set_pause(file->score->curr_play_id, false);
        }
        Audiolib_Music.paused = false;
        Audiolib_music_cs.leave();
        Audiolib_music_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    Audiolib_music_cs.leave();
    return AUDIOLIB_ERR_NOT_PLAYING;
}

// 0x00488C10 (:3269)
void audiolib_interactive_music_system_set_limited_level(bool level_set)
{
    Audiolib_Music_Level_Limited = level_set;
}

// 0x00488C20 (:3274)
void audiolib_music_set_fsm_state(bool in_fsm)
{
    // The music plays at 70 % in the FSM (front end menus).
    Audiolib_Music.fsm_scalar = in_fsm ? 0.7f : 1.0f;
}

// 0x00495890 (:3285)
bool audiolib_interactive_music_enough_time_to_fade_combat_cue()
{
    Audiolib_music_cs.enter();
    int marker_time = -1;
    int start_time = -1;
    char* marker_text;
    const audiolib_music_file_data* file = audiolib_internal_music_id_to_ptr(Audiolib_Music.current_music_id);
    if (file != nullptr && file->score != nullptr &&
        audiolib_marker_get_next_marker(file->score->curr_play_id, &marker_time, &start_time, &marker_text) == AUDIOLIB_ERR_NONE &&
        toupper(*marker_text) == 'E' && start_time + marker_time - timer_get(TIMER_MILLISECONDS) < 5000)
    {
        // The end marker within 5 s.
        Audiolib_music_cs.leave();
        return false;
    }
    Audiolib_music_cs.leave();
    return true;
}
