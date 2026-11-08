// Original: volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h
//
// Voice persona and voice line handles, persona groups, and the persona line record of the voice persona files (its
// inline methods are in this header). Placement of the handle and group enums inferred: voice_line_handle sits here
// because the persona line returns it inline and human_voice_shared.h includes this header.
#pragma once

class timestamp;

enum voice_line_handle
{
    INVALID_VOICE_LINE_HANDLE = -1,
    VOICE_LINE_HANDLE_IS_DLC = 32768,
    VOICE_LINE_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

enum human_voice_persona_handle
{
    INVALID_HUMAN_VOICE_PERSONA_HANDLE = -1,
    HUMAN_VOICE_PERSONA_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

enum human_voice_persona_groups
{
    HVPG_ANY = -1,
    HVPG_GUERILLA = 0,
    HVPG_EDF = 1,
    HVPG_MARAUDER = 2,
    NUM_HUMAN_VOICE_PERSONA_GROUPS = 3,
};

// size 0xC
struct human_voice_persona_line
{
    human_voice_persona_line(); // 0x00A0E000 (human_voice_shared.cpp:141)
    bool game_init(); // 0x00A0E0C0 (human_voice_shared.cpp:270)
    const timestamp& cooldown_timer() const; // 0x00A0D5F0 (.h:60)
    timestamp& cooldown_timer(); // 0x00A0D5E0 (.h:59)
    voice_line_handle get_voice_line_handle() const; // 0x00A0D600 (.h:64)

    unsigned int name_crc_or_timer; // 0x00
    unsigned short variation_start_offset; // 0x04
    unsigned short descriptor_idx; // 0x06
    unsigned char num_variations; // 0x08
    unsigned char next_variation; // 0x09
};
