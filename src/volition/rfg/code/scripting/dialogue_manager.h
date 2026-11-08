// Original: volition/rfg/code/scripting/dialogue_manager.h
// Implementation: volition/rfg/code/scripting/dialogue_manager.cpp
//
// The dialogue manager: a dialogue is a sequence of lines (elements) spoken by humans, the radio or public address
// speakers, queued by priority (ambient up to briefing), paused and cancelled by scripts, vehicles and briefings. The
// multiplayer announcer lines hold a dialogue priority. Placement of dialogue_priorities inferred
// (dialogue_manager_start_dialogue takes it).
#pragma once

#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/vlib/code/time/timestamp.h"

struct dialogue_info;
class human;
class human_voice_persona;

enum dialogue_priorities
{
    DIALOGUE_PRIORITY_UNDEFINED = -1,
    DIALOGUE_PRIORITY_AMBIENT = 0,
    DIALOGUE_PRIORITY_RADIO = 1,
    DIALOGUE_PRIORITY_NORMAL = 2,
    DIALOGUE_PRIORITY_HIGH = 3,
    DIALOGUE_PRIORITY_BRIEFING = 4,
    DIALOGUE_PRIORITY_CURRENT = 5,
    NUM_DIALOGUE_PRIORITIES = 6,
};

enum dialogue_element_play_status
{
    DEPS_INACTIVE = 0,
    DEPS_WAITING = 1,
    DEPS_QUEUE_ME = 2,
    DEPS_QUEUED = 3,
    DEPS_PLAYING = 4,
    DEPS_DONE = 5,
    NUM_DIALOGUE_ELEMENT_PLAY_STATUS = 6,
};

// size 0x48
struct dialogue_element
{
    dialogue_element(); // 0x00B2D640 (:82)
    void clear(); // 0x00B23890 (:150)
    bool has_active_radio_message() const;

    dialogue_info* owner; // 0x00
    dialogue_element* parent; // 0x04
    dialogue_element* child_ptr; // 0x08
    dialogue_element* child_prev; // 0x0C
    dialogue_element* child_next; // 0x10
    voice_line_handle line_to_play; // 0x14
    unsigned int target_human; // 0x18
    unsigned int target_automobile; // 0x1C
    const human_voice_persona* target_persona; // 0x20
    int time_offset; // 0x24
    timestamp time_to_play; // 0x28
    public_address_group_handle pa_group; // 0x2C
    const char* message_icon_name; // 0x30
    unsigned short radio_instance; // 0x34
    int play_instance; // 0x38
    int play_time; // 0x3C
    dialogue_element_play_status play_status; // 0x40
    bool play_intro; // 0x44
    bool play_outro; // 0x45
};

// size 0x8
struct dialogue_info
{
    dialogue_info(); // 0x00B2D690 (:99)
    void clear(); // 0x00B23860 (:101)

    dialogue_element* root; // 0x00
    unsigned char priority; // 0x04
    bool paused; // 0x05
};

bool pause_dialogue_elements(dialogue_element* dep); // 0x00B2D6C0 (:271)
void pause_dialogue(dialogue_info* dip); // 0x00B2D790 (:320)
void unpause_dialogue(dialogue_info* dip); // 0x00B238E0 (:330)
bool check_queued_line_status(dialogue_element* dep); // 0x00B2D950 (:422)
void process_dialogue(dialogue_info* dip, bool& duck_me); // 0x00B3A0E0 (:591)
dialogue_info* find_next_dialogue_to_play(); // 0x00B330E0 (:613)
void dialogue_manager_level_init_pre(); // 0x00B3D760 (:700)
void dialogue_manager_do_frame(); // 0x00B3D7C0 (:716)
bool dialogue_manager_start_dialogue(voice_line_handle line_handle, human* hp, dialogue_priorities priority); // 0x00B3A1A0 (:883)
bool dialogue_manager_start_dialogue(voice_line_handle line_handle, const human_voice_persona* hvpp, dialogue_priorities priority, public_address_group_handle pa_group, const char* message_icon_name); // 0x00B3A230 (:910)
bool dialogue_manager_start_dialogue_megaphone(voice_line_handle line_handle, const human_voice_persona* hvpp, dialogue_priorities priority, unsigned int automobile_handle); // 0x00B3A2C0 (:938)
bool dialogue_manager_add_line(voice_line_handle line_handle, human* hp, voice_line_handle parent_line, int time_offset); // 0x00B331D0 (:964)
bool dialogue_manager_add_line(voice_line_handle line_handle, const human_voice_persona* hvpp, voice_line_handle parent_line, int time_offset, public_address_group_handle pa_group, const char* message_icon_name); // 0x00B33290 (:1001)
void dialogue_manager_pause(); // 0x00B33340 (:1046)
void dialogue_manager_unpause(); // 0x00B333F0 (:1060)
void dialogue_manager_cancel(voice_line_handle line_handle, bool immediate_stop); // 0x00B3A340 (:1081)
void dialogue_manager_cancel_vehicle(unsigned int vehicle_handle); // 0x00B3A430 (:1096)
void dialogue_manager_cancel_all(); // 0x00B3A520 (:1119)
void dialogue_manager_cancel_briefing(); // 0x00B3A5F0 (:1131)
bool dialogue_manager_is_dialogue_playing(voice_line_handle line_handle); // 0x00B334A0 (:1151)
int dialogue_manager_get_num_dialogues(dialogue_priorities which); // 0x00B33560 (:1175)
int dialogue_manager_get_num_free_dialogues(); // 0x00B2DA40 (:1190)
bool dialogue_manager_is_any_line_queued(const human* hp, bool include_playing_lines); // 0x00B335A0 (:1205)
