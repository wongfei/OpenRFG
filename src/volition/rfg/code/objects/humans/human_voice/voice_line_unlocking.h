// Original: volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h
// Implementation: volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.cpp
//
// Voice lines unlocked by progress for a few characters (Samanya, the old coot, the miners, guerrilla VIPs), chosen with
// a global cooldown. Placement of the enums inferred.
#pragma once

#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"

class human;

enum vlu_groups
{
    VLUG_SAMANYA = 0,
    VLUG_OLD_COOT = 1,
    VLUG_AMB_MINER = 2,
    VLUG_GUERRILLA_VIP = 3,
    NUM_VLU_GROUPS = 4,
};

enum voice_line_unlocking_handle
{
    INVALID_VOICE_LINE_UNLOCKING_HANDLE = -1,
    VOICE_LINE_UNLOCKING_HANDLE_FORCE_TO_32_BIT = 0x7FFFFFFF,
};

// size 0x28
struct voice_line_unlocking_line
{
    voice_line_unlocking_line(); // 0x00A0E1B0 (:41)

    voice_line_handle line_handle; // 0x00
    unsigned int unlock_after_district_hash_1; // 0x04
    unsigned int unlock_after_district_hash_2; // 0x08
    unsigned int lock_after_district_hash_1; // 0x0C
    unsigned int lock_after_district_hash_2; // 0x10
    mission_handle unlock_after; // 0x14
    mission_handle lock_after; // 0x18
    unsigned int unlock_after_purchase; // 0x1C
    unsigned int lock_after_purchase; // 0x20
    human_teams unlock_after_friendly; // 0x24
};

// size 0x4C
struct voice_line_unlocking_group
{
    voice_line_unlocking_group(); // 0x00A0E1D0 (:54)

    char name[64]; // 0x00
    voice_line_unlocking_line* lines; // 0x40
    int num_lines; // 0x44
    int cur_line_idx; // 0x48
};

void voice_line_unlocking_system_init(); // 0x00A0E230 (:93)
voice_line_unlocking_handle voice_line_unlocking_get_handle_slow(const char* name); // 0x00A0E620 (:254)
voice_line_unlocking_handle voice_line_unlocking_get_handle(vlu_groups group); // 0x00A0E650 (:266)
voice_line_handle voice_line_unlocking_choose(voice_line_unlocking_handle vlu_handle); // 0x00A32D60 (:282)
voice_line_handle voice_line_unlocking_choose(vlu_groups group); // 0x00A32F90 (:394)
void voice_line_unlocking_set_global_cooldown(human* hp, voice_line_unlocking_handle group_handle, int cooldown_ms); // 0x00A32FB0 (:400)
void voice_line_unlocking_set_global_cooldown(human* hp, vlu_groups group, int cooldown_ms); // 0x00A33010 (:420)
void voice_line_unlocking_set_global_cooldown_slow(human* hp, const char* group_name, int cooldown_ms); // 0x00A33030 (:425)
