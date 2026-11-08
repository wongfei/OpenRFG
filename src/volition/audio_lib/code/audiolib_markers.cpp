// Original: volition/audio_lib/code/audiolib_markers.cpp
//
// Cue markers (SFX.md SX2): foley_markers.xtbl lists marker times and texts per cue. A play of a marked cue with
// callback flag 0x40 takes a process list node; audiolib_marker_process fires a marker callback (action 0x40,
// "<first text character>:<time>") 40 ms ahead of each marker, on the 10 ms grid. A marker id packs the node index (low
// half) and the node's rolling counter (high half). Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_markers.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_callbacks.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdlib.h>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0x8
struct audiolib_marker_file_list
{
    int num_files; // 0x00
    audiolib_marker_file_data* first_file; // 0x04
};
static_assert(sizeof(audiolib_marker_file_list) == 0x8, "audiolib_marker_file_list");

// ---- public globals (rfg.pdb; no header declares them)
// The text of a marker without one.
char music_marker_text[4] = "E"; // 0x014F9318
object_pool<audiolib_marker_process_list_node,20,1> Audiolib_marker_process_list(nullptr, &audiolib_marker_process_list_node::drop, 0x49); // 0x01A9B0B0

// ---- file-static globals
// bool Audiolib_markers_inited (0x01A79A8B) is used here too; placed in audiolib.cpp.
static audiolib_marker_file_list Audiolib_Marked_files; // 0x01A79AD8
static cs_wrapper Audiolib_marker_cs(0); // 0x01A988A0

// 0x00488390 (:89)
void audiolib_marker_process_list_node::invalidate()
{
    play_id = -1;
}

// 0x004883A0 (:95)
audiolib_marker_process_list_node::audiolib_marker_process_list_node()
{
    // The rolling counter keeps 16 bits (the high half of a marker id).
    rolling_counter &= 0xFFFF;
    play_id = -1;
    if (rolling_counter == 0)
    {
        rolling_counter = 1;
    }
}

// 0x004883D0 (:135)
void audiolib_marker_process_list_node::drop()
{
    // Audiolib_marker_process_list's release callback.
    ++rolling_counter;
    play_id = -1;
    if (rolling_counter > 0xFFFF)
    {
        rolling_counter = 1;
    }
}

// 0x004883F0 (:149), root in EAX
static audiolib_result audiolib_internal_parse_marker_file(xml_element* root)
{
    for (xml_element* file = xml_find(root, "File"); file != nullptr; file = xml_find_next(root, file, "File"))
    {
        // One block per file: the file data, the markers, then their texts. Original: neither count is checked.
        const int num_markers = atoi(xml_table_get_req_string_ref(file, "Number_of_markers"));
        const int text_size = atoi(xml_table_get_req_string_ref(file, "Total_text_size"));
        const unsigned int size = text_size + sizeof(audiolib_marker_file_data) + num_markers * sizeof(audiolib_marker_node);
        audiolib_marker_file_data* data = (audiolib_marker_file_data*)Audiolib_state.pool->alloc(size, 0);
        memset(data, 0, size);
        data->markers = (audiolib_marker_node*)(data + 1);
        char* text = (char*)(data->markers + num_markers);
        const char* name = xml_table_get_req_string_ref(file, "Name");
        strncpy(data->name, name, sizeof(data->name));
        data->name_crc = crc_str(name, 0);
        // Appended to the list.
        if (Audiolib_Marked_files.first_file == nullptr)
        {
            Audiolib_Marked_files.first_file = data;
        }
        else
        {
            audiolib_marker_file_data* last = Audiolib_Marked_files.first_file;
            while (last->next_file != nullptr)
            {
                last = last->next_file;
            }
            last->next_file = data;
        }
        ++Audiolib_Marked_files.num_files;
        data->num_markers = 0;
        for (xml_element* marker = xml_find(file, "Marker"); marker != nullptr; marker = xml_find_next(file, marker, "Marker"))
        {
            const char* time_offset = xml_table_get_req_string_ref(marker, "Time_Offset");
            const char* sample_offset = xml_table_get_req_string_ref(marker, "Sample_Offset");
            const char* marker_text = xml_table_get_opt_string_ref(marker, "Text");
            if (marker_text == nullptr)
            {
                marker_text = music_marker_text;
            }
            if (sample_offset == nullptr && time_offset == nullptr)
            {
                continue;
            }
            audiolib_marker_node& node = data->markers[data->num_markers];
            node.offset = sample_offset != nullptr ? atol(sample_offset) : 0;
            node.text = text;
            node.time = time_offset != nullptr ? atol(time_offset) : 0;
            strcpy(text, marker_text);
            text += strlen(text) + 1;
            ++data->num_markers;
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// The next trigger of a marker: 40 ms early, on the 10 ms grid of the play's start.
static int audiolib_marker_trigger_time(const audiolib_marker_process_list_node* node, int start_time)
{
    const int time = node->curr_marker->time;
    return time - (time - node->offset_time + start_time) % 10 - node->offset_time - 40 + start_time;
}

// 0x0049B4C0 (:244)
bool audiolib_marker_process_function(void* user_data, int curr_time, int time_step)
{
    (void)user_data;
    (void)time_step;
    Audiolib_marker_cs.enter();
    Audiolib_marker_process_list.thread_access.enter();
    audiolib_marker_process_list_node* next = Audiolib_marker_process_list.get_first(0);
    while (next != nullptr)
    {
        audiolib_marker_process_list_node* node = next;
        next = Audiolib_marker_process_list.get_next(node);
        if (node->remove || !Audiolib_state.enabled)
        {
            Audiolib_marker_process_list.free(node);
            continue;
        }
        if (node->paused || node->marker_data.num_markers <= 0)
        {
            continue;
        }
        // An extra marker before the next one comes first.
        const int extra_time = node->extra_trigger_time;
        if (extra_time > 0 && extra_time < node->next_trigger_time)
        {
            node->next_trigger_time = extra_time;
        }
        const int trigger_time = node->next_trigger_time;
        if (trigger_time < 0)
        {
            continue;
        }
        if (trigger_time == 0)
        {
            // The play started.
            node->start_time = curr_time;
            if (node->curr_marker == nullptr)
            {
                node->curr_marker = node->marker_data.markers;
            }
            node->next_trigger_time = audiolib_marker_trigger_time(node, curr_time);
            continue;
        }
        if (trigger_time > curr_time)
        {
            continue;
        }
        const char* text = trigger_time == extra_time ? node->extra_trigger_text : node->curr_marker->text;
        AUDIOLIB_CUE_CALLBACK_DATA callback_event;
        memset(&callback_event, 0, sizeof(callback_event));
        callback_event.play_id = node->play_id;
        callback_event.action = 0x40;
        callback_event.time = curr_time;
        if (text != nullptr)
        {
            _snprintf_safe(callback_event.marker_text, sizeof(callback_event.marker_text), "%c:%d", text[0], node->curr_marker->time);
        }
        audiolib_internal_callback_event_add(&callback_event);
        // Original: an extra marker also moves on to the next marker.
        const int index = node->curr_marker_index + 1;
        if (index < node->marker_data.num_markers)
        {
            node->curr_marker_index = index;
            node->curr_marker = &node->marker_data.markers[index];
            node->extra_trigger_time = -1;
            node->next_trigger_time = audiolib_marker_trigger_time(node, node->start_time);
        }
        else
        {
            node->next_trigger_time = -1;
            node->extra_trigger_time = -1;
        }
    }
    Audiolib_marker_process_list.thread_access.leave();
    Audiolib_marker_cs.leave();
    return false;
}

// 0x00488620 (:341)
audiolib_marker_file_data* audiolib_marker_internal_find_marker_data(unsigned int name_crc)
{
    Audiolib_marker_cs.enter();
    audiolib_marker_file_data* file = Audiolib_Marked_files.first_file;
    while (file != nullptr && file->name_crc != name_crc)
    {
        file = file->next_file;
    }
    Audiolib_marker_cs.leave();
    return file;
}

// 0x0049B730 (:361)
bool audiolib_marker_system_init()
{
    if (Audiolib_markers_inited)
    {
        return true;
    }
    Audiolib_markers_inited = false;
    Audiolib_Marked_files.num_files = 0;
    Audiolib_Marked_files.first_file = nullptr;
    char path[256];
    strncpy(path, Audiolib_state.root_dir, sizeof(path));
    strncat(path, Audiolib_state.table_dir, sizeof(path));
    strncat(path, "foley_markers.xtbl", sizeof(path));
    xml_element* root = xml_table_parse(path);
    if (root != nullptr)
    {
        if (audiolib_internal_parse_marker_file(root) == AUDIOLIB_ERR_NONE)
        {
            Audiolib_markers_inited = true;
        }
        xml_table_close(root);
    }
    // audiolib_thread_function_register, inlined. Original: the process is registered without a table too.
    audiolib_process_data* process = Audio_process_list.alloc(0);
    if (process == nullptr)
    {
        Audiolib_markers_inited = false;
    }
    else
    {
        process->user_data = nullptr;
        process->function = audiolib_marker_process_function;
        strncpy(process->func_name, "audiolib_marker_process", sizeof(process->func_name));
    }
    return Audiolib_markers_inited;
}

// 0x00488660 (:393)
void audiolib_marker_system_shutdown()
{
    // The process stays registered and the files allocated (the audio pool goes with the system).
    Audiolib_markers_inited = false;
}

// 0x00495260 (:402)
audiolib_result audiolib_marker_add_marker_to_process_list(audiolib_play_data* play)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_marker_cs.enter();
    // A cue without markers needs no node.
    const audiolib_marker_file_data* file = audiolib_marker_internal_find_marker_data(play->foley_data.name_crc);
    if (file != nullptr && file->num_markers > 0)
    {
        audiolib_marker_process_list_node* node = Audiolib_marker_process_list.alloc(0);
        if (node == nullptr)
        {
            Audiolib_marker_cs.leave();
            return AUDIOLIB_ERR_NO_MARKER_SLOTS;
        }
        // Paused until the play starts.
        node->play_id = play->play_id;
        node->remove = false;
        node->offset_time = play->params.offset_ms;
        node->next_trigger_time = 0;
        node->marker_data.num_markers = file->num_markers;
        node->marker_data.markers = file->markers;
        node->curr_marker = file->markers;
        node->curr_marker_index = 0;
        node->extra_trigger_time = 0;
        node->extra_trigger_text[0] = '\0';
        node->paused = true;
        play->marker_id = (Audiolib_marker_process_list.get_index(node) & 0xFFFF) | (node->rolling_counter << 16);
    }
    Audiolib_marker_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// The node of a marker id (no rolling counter check in rfg.exe).
static audiolib_marker_process_list_node* audiolib_marker_id_to_node(int marker_id)
{
    return Audiolib_marker_process_list.get_entry(marker_id & 0xFFFF);
}

// 0x0048FA80 (:449)
audiolib_result audiolib_marker_remove_marker_from_process_list(audiolib_play_data* play)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->marker_id != -1)
    {
        // The process frees the node.
        Audiolib_marker_cs.enter();
        audiolib_marker_process_list_node* node = audiolib_marker_id_to_node(play->marker_id);
        if (node != nullptr && !node->remove)
        {
            play->marker_id = -1;
            node->remove = true;
        }
        Audiolib_marker_cs.leave();
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x00495350 (:480)
audiolib_result audiolib_marker_add_marker_to_data(audiolib_play_data* play)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_marker_add_marker_to_process_list(play);
}

// 0x0048FAF0 (:490)
audiolib_result audiolib_marker_remove_marker_data(audiolib_play_data* play)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    return audiolib_marker_remove_marker_from_process_list(play);
}

// 0x0048FB20 (:501)
audiolib_result audiolib_marker_pause_marker_data(audiolib_play_data* play, bool pause)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->marker_id == -1)
    {
        return AUDIOLIB_ERR_NONE;
    }
    Audiolib_marker_cs.enter();
    const int now = timer_get(TIMER_MILLISECONDS);
    audiolib_marker_process_list_node* node = audiolib_marker_id_to_node(play->marker_id);
    if (node != nullptr)
    {
        if (pause)
        {
            node->paused = true;
            node->paused_time = now;
            Audiolib_marker_cs.leave();
            return AUDIOLIB_ERR_NONE;
        }
        // The pause does not count as play time.
        if (node->paused_time > 0)
        {
            const int paused = now - node->paused_time;
            node->start_time += paused;
            node->next_trigger_time += paused;
            if (node->extra_trigger_time > 0)
            {
                node->extra_trigger_time += paused;
            }
            node->paused_time = 0;
        }
        node->paused = false;
    }
    Audiolib_marker_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// The marker id of a play (-1 without markers or play).
static bool audiolib_marker_play_marker_id(int play_id, int& marker_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return false;
    }
    marker_id = play->marker_id;
    Audiolib_play_cs.leave();
    return true;
}

// 0x00495380 (:543)
audiolib_result audiolib_marker_get_next_marker(int play_id, int* time, int* start_time, char** text)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    int marker_id;
    if (!audiolib_marker_play_marker_id(play_id, marker_id) || marker_id == -1)
    {
        return AUDIOLIB_ERR_NO_MARKERS;
    }
    Audiolib_marker_cs.enter();
    audiolib_marker_process_list_node* node = audiolib_marker_id_to_node(marker_id);
    if (node == nullptr)
    {
        Audiolib_marker_cs.leave();
        return AUDIOLIB_ERR_NO_MARKERS;
    }
    if (time != nullptr)
    {
        *time = node->curr_marker->time;
    }
    if (start_time != nullptr)
    {
        *start_time = node->start_time;
    }
    if (text != nullptr)
    {
        *text = node->curr_marker->text;
    }
    Audiolib_marker_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00495430 (:574)
audiolib_result audiolib_play_add_extra_marker(int play_id, int time, const char* text)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    int marker_id;
    if (!audiolib_marker_play_marker_id(play_id, marker_id) || marker_id == -1)
    {
        return AUDIOLIB_ERR_NO_MARKERS;
    }
    Audiolib_marker_cs.enter();
    // One extra marker at a time, at a time into the cue.
    audiolib_marker_process_list_node* node = audiolib_marker_id_to_node(marker_id);
    if (node != nullptr && node->extra_trigger_time < 1)
    {
        if (text == nullptr)
        {
            text = "extra";
        }
        strncpy(node->extra_trigger_text, text, sizeof(node->extra_trigger_text));
        node->extra_trigger_time = node->start_time - node->offset_time + time;
    }
    Audiolib_marker_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00488670 (:621)
audiolib_result audiolib_cue_get_num_markers(int cue_id, int* num_markers)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (num_markers == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    const audiolib_foley_data* foley = cue_id != -1 ? audiolib_get_foley_data_pointer(cue_id) : nullptr;
    if (foley == nullptr)
    {
        return AUDIOLIB_ERR_BAD_CUE_ID;
    }
    const audiolib_marker_file_data* file = audiolib_marker_internal_find_marker_data(foley->name_crc);
    if (file == nullptr)
    {
        return AUDIOLIB_ERR_MARKERS_NOT_FOUND;
    }
    *num_markers = file->num_markers;
    return AUDIOLIB_ERR_NONE;
}

// 0x004886D0 (:650)
audiolib_result audiolib_cue_get_marker_info(int cue_id, int marker_index, int* marker_offset_ms, char** marker_text)
{
    if (!Audiolib_markers_inited)
    {
        return AUDIOLIB_ERR_NOT_INITED;
    }
    if (marker_offset_ms == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    const audiolib_foley_data* foley = cue_id != -1 ? audiolib_get_foley_data_pointer(cue_id) : nullptr;
    if (foley == nullptr)
    {
        return AUDIOLIB_ERR_BAD_CUE_ID;
    }
    const audiolib_marker_file_data* file = audiolib_marker_internal_find_marker_data(foley->name_crc);
    if (file == nullptr)
    {
        return AUDIOLIB_ERR_MARKERS_NOT_FOUND;
    }
    // Original: an index past the end succeeds with nothing written.
    if (marker_index < file->num_markers)
    {
        *marker_offset_ms = file->markers[marker_index].time;
        if (marker_text != nullptr)
        {
            *marker_text = file->markers[marker_index].text;
        }
    }
    return AUDIOLIB_ERR_NONE;
}
