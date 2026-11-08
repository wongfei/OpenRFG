// Original: volition/rfg/code/multi/multi_game/multi_game_announcer.cpp
//
// The multiplayer announcer (decompiled): a line plays at most once per its minimum time, queued in up to five pending
// slots (two for normal priority ones) while dialogues play; a briefing line clears the queue and the dialogues. The
// next pending line starts through the dialogue manager on the general announcer persona (MPANC) when no dialogue
// plays (multi_game_process inlines that step). The external interface is declared in multi_game_announcer.h.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared by
// the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py). SFX.md SX6: ported
// whole.

#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"

#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"

// ---- file-static globals
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
// const char* Multi_general_announcer_persona_name (0x0167E508) is used here too; placed in multi_game_mode_common.cpp.
// multi_announcer_line_info Multi_announcer_lines[52] (0x01691F00) is used here too; placed in multi_game_mode_capture_the_flag.cpp.
// farray<backpack_rack_popup,4> Backpack_rack_popups (0x01692588) is used here too; placed in multi_game_mode_common.cpp.
// int Mp_announce_num_pending_dialogues (0x02FEB668) is used here too; placed in multi_game_mode_common.cpp.

// The announcer lines waiting for the dialogue manager (declared in multi_game_announcer.h; multi_game_process plays
// them too).
pending_dialogue_info Pending_dialogues[5]; // 0x02FE738C

// Magic numbers: at most five lines wait or play, and at most three when the new line has normal priority.
static const int MAX_PENDING_LINES = 5;
static const int MAX_PENDING_NORMAL_LINES = 3;

// 0x0094EE50 (:53)
multi_announcer_line_info::multi_announcer_line_info(const char* name, const char* response_name, int min_time_between_plays_ms_in, dialogue_priorities priority_in, int supress_group)
    : line_name(name), team_response(response_name)
{
    priority = priority_in;
    supress_line_group = supress_group;
    min_time_between_plays_ms = min_time_between_plays_ms_in;
    last_played_time.invalidate();
}

// 0x0094EEA0 (:176): a debug check that every line resolves (the handles are not used).
void unused_dcf_validate_mp_announce()
{
    for (int i = 0; i < NUM_MULTI_ANNOUNCER_LINES; i++)
    {
        if (Multi_announcer_lines[i].line_name.str != nullptr)
        {
            voice_line_handle_from_crc(Multi_announcer_lines[i].line_name.str_checksum);
        }
    }
}

// ---- internal functions

// 0x0094EED0 (:190). LTCG: line in EAX. The line starts as a dialogue of its priority on the announcer persona.
static void mga_play_line(multi_announcer_lines line)
{
    human_voice_persona* hvpp = human_voice_persona_from_handle(Multi_general_announcer);
    if (hvpp != nullptr)
    {
        dialogue_manager_start_dialogue(voice_line_handle_from_crc(Multi_announcer_lines[line].line_name.str_checksum), hvpp, Multi_announcer_lines[line].priority, INVALID_PA_HANDLE, nullptr);
    }
}

// 0x0094EF20 (:217). LTCG: index in EAX.
static void mga_remove_pending_line(int index)
{
    if (index < 0 || index >= Mp_announce_num_pending_dialogues)
    {
        return;
    }
    Mp_announce_num_pending_dialogues--;
    for (int i = index; i < Mp_announce_num_pending_dialogues; i++)
    {
        Pending_dialogues[i] = Pending_dialogues[i + 1];
    }
}

// 0x0094EF60 (:227). LTCG: line in EAX, target_handle on the stack. The pending lines for the same target in the
// line's suppression group give way to it. Original: the line after a removed one is skipped (the index moves on).
static void mga_check_removal(multi_announcer_lines line, unsigned int target_handle)
{
    const int group = Multi_announcer_lines[line].supress_line_group;
    for (int i = 0; i < Mp_announce_num_pending_dialogues; i++)
    {
        if (Pending_dialogues[i].handle == target_handle && group != -1 && Multi_announcer_lines[Pending_dialogues[i].line].supress_line_group == group)
        {
            mga_remove_pending_line(i);
        }
    }
}

// 0x0094EFE0 (:244). LTCG: line in EAX. Inlined in multi_game_announcer_play_line.
static bool mga_too_many_pending_lines(multi_announcer_lines line)
{
    const int count = dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) + Mp_announce_num_pending_dialogues;
    return count >= MAX_PENDING_LINES || (Multi_announcer_lines[line].priority == DIALOGUE_PRIORITY_NORMAL && count >= MAX_PENDING_NORMAL_LINES);
}

// ---- external functions

// 0x0094F020 (:267)
void multi_game_announcer_match_init()
{
    Multi_general_announcer = human_voice_persona_handle_from_name(Multi_general_announcer_persona_name);
    Mp_announce_num_pending_dialogues = 0;
}

// 0x0094F040 (:283): every line can play again.
void multi_game_announcer_round_init()
{
    for (int i = 0; i < NUM_MULTI_ANNOUNCER_LINES; i++)
    {
        Multi_announcer_lines[i].last_played_time.invalidate();
    }
}

// 0x0094F070 (:294)
void multi_game_announcer_play_line(multi_announcer_lines line, unsigned int target_handle)
{
    if ((unsigned int)line >= NUM_MULTI_ANNOUNCER_LINES)
    {
        return;
    }
    multi_announcer_line_info& info = Multi_announcer_lines[line];
    if (info.last_played_time.valid() && info.last_played_time.time_since() < info.min_time_between_plays_ms)
    {
        return;
    }
    info.last_played_time.set(0);
    if (info.priority == DIALOGUE_PRIORITY_BRIEFING)
    {
        Mp_announce_num_pending_dialogues = 0;
        dialogue_manager_cancel_all();
    }
    mga_check_removal(line, target_handle);
    if (!mga_too_many_pending_lines(line))
    {
        Pending_dialogues[Mp_announce_num_pending_dialogues].line = line;
        Pending_dialogues[Mp_announce_num_pending_dialogues].handle = target_handle;
        Mp_announce_num_pending_dialogues++;
    }
}

// 0x0094F120 (:331)
bool multi_game_announcer_line_played_or_pending_this_round(multi_announcer_lines line)
{
    if ((unsigned int)line >= NUM_MULTI_ANNOUNCER_LINES)
    {
        return false;
    }
    return Multi_announcer_lines[line].last_played_time.valid();
}

// 0x0094F140 (:338): the first pending line starts when no dialogue plays.
void multi_game_announcer_process()
{
    if (Mp_announce_num_pending_dialogues > 0 && dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
    {
        mga_play_line(Pending_dialogues[0].line);
        mga_remove_pending_line(0);
    }
}
