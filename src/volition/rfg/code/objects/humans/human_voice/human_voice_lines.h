// Original: volition/rfg/code/objects/humans/human_voice/human_voice_lines.h
// Implementation: volition/rfg/code/objects/humans/human_voice/human_voice_lines.cpp
//
// The voice line table (from XML): per line its group, priority, cooldowns and flags; lookups between line handles,
// names, CRCs and voice_lines values.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/time/timestamp.h"

struct human_voice_line_info;
class mempool;

// size 0x8
struct voice_line_lookup_entry
{
    voice_line_lookup_entry(const char* name_); // 0x00A0DA70 (:52)

    const char* name; // 0x00
    human_voice_line_info* descriptor; // 0x04
};

// size 0x10
struct human_voice_line_group_info
{
    // size 0x2
    struct human_voice_line_info_flags
    {
        unsigned char valid : 1; // 0x00:0
        unsigned char dummy : 1; // 0x00:1
        unsigned char edf_projected : 1; // 0x00:2
        unsigned char pa_projected : 1; // 0x00:3
        unsigned char spk_projected : 1; // 0x00:4
        unsigned char gur_radio_far : 1; // 0x00:5
        unsigned char gur_radio_local : 1; // 0x00:6
        unsigned char gur_pc_transmit : 1; // 0x00:7
        unsigned char mp_announcer : 1; // 0x01:0
        unsigned char subtitled : 1; // 0x01:1
        unsigned char force_dialogue : 1; // 0x01:2
        unsigned char clear_or_radio : 1; // 0x01:3
    };

    voice_line_priorities get_priority() const;
    bool play_2d() const; // 0x00A06920 (.h:59)
    human_voice_line_group_info(); // 0x00A0DC60 (:785)

    timestamp global_cooldown_timer; // 0x00
    unsigned short local_cooldown; // 0x04
    unsigned short global_cooldown; // 0x06
    unsigned short min_delay; // 0x08
    unsigned short max_delay; // 0x0A
    unsigned char play_chance_pct; // 0x0C
    unsigned char default_priority; // 0x0D
    human_voice_line_info_flags flags; // 0x0E
};

// size 0x38
struct human_voice_line_info
{
    human_voice_line_info(); // 0x00A0DC90 (:794)
    bool global_cooldown_elapsed(human_voice_persona_groups) const;
    int global_cooldown_time_until(human_voice_persona_groups group) const; // 0x00A06940 (.h:92)
    void set_global_cooldown(human_voice_persona_groups);
    const char* get_name() const; // 0x00A06960 (.h:101)

    checksum_stri name_crc; // 0x00
    unsigned short max_simultaneous; // 0x04
    human_voice_line_group_info group_descriptors[3]; // 0x08
};

// File statics shared by the humans unity TU of rfg.exe (human.cpp and the human_voice files): external here, each
// defined in its first user in path order (human.cpp, human_voice.cpp, human_voice_lines.cpp).
extern voice_line_lookup_entry Voice_line_lookup_table[129]; // 0x01695018 (human.cpp), one per voice_lines value
extern human_voice_line_info* Voice_line_descriptors; // 0x0302F08C (human.cpp)
extern int Num_voice_line_descriptors; // 0x0302F088 (human_voice.cpp)
extern int Num_dlc_voice_line_descriptors; // 0x0302F090 (human_voice.cpp)
extern human_voice_line_info* Dlc_voice_line_descriptors; // 0x0302F094 (human_voice.cpp)
extern char last_voice_line_name[32]; // 0x0301A1F8 (human_voice_lines.cpp), the get_name text

// template instance: void swap_voice_lines<unsigned short>(unsigned short& a, unsigned short& b); // 0x00A24770 (:35)
// template instance: void get_xml_uint<unsigned short>(unsigned short& val, xml_element* root, const char* tag_name, unsigned int min, unsigned int max); // 0x00A24720 (:421)
// template instance: void get_xml_uint<unsigned char>(unsigned char& val, xml_element* root, const char* tag_name, unsigned int min, unsigned int max); // 0x00A24790 (:421)
void human_voice_lines_dlc_shutdown(); // 0x00A0DB50 (:686)
bool human_voice_lines_parse_table(const char* table_name, bool dlc, mempool* pool); // 0x00A32830 (:809)
human_voice_persona_groups human_voice_persona_group_from_team(human_teams team); // 0x00A32890 (:840)
human_voice_line_info* human_voice_line_get_info(voice_lines line); // 0x00A0DCE0 (:856)
human_voice_line_info* human_voice_line_get_info(const char* line_name); // 0x00A0DD00 (:861)
human_voice_line_info* human_voice_line_get_info(voice_line_handle line_handle); // 0x00A0DD30 (:865)
voice_line_handle voice_line_handle_from_crc(checksum_stri name_crc); // 0x00A0DD80 (:881)
voice_line_handle voice_line_handle_from_name(const char* line_name); // 0x00A0DE00 (:903)
voice_line_handle voice_line_handle_from_line(voice_lines line); // 0x00A0DEA0 (:930)
voice_lines voice_line_from_name(const char* line_name); // 0x00A0DEE0 (:943)
const char* voice_line_handle_to_name(voice_line_handle line_handle); // 0x00A0DF20 (:958)

// 0x00A0DA70 (:52)
inline voice_line_lookup_entry::voice_line_lookup_entry(const char* name_)
    : name(name_)
    , descriptor(nullptr)
{
}
