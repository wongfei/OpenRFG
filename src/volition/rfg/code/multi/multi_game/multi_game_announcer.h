// Original: volition/rfg/code/multi/multi_game/multi_game_announcer.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_announcer.cpp
//
// The announcer: voice lines played on match and round events (multi_announcer_lines), once per round. Placement of the
// line enum inferred.
#pragma once

#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/time/timestamp.h"

enum multi_announcer_lines
{
    INVALID_MULTI_ANNOUNCER_LINE = -1,
    MAL_GEN_ROUND_OVER = 0,
    MAL_GEN_MATCH_OVER = 1,
    MAL_GEN_EDF_WIN_MATCH = 2,
    MAL_GEN_GUERRILLAS_WIN_MATCH = 3,
    MAL_GEN_ALLY_WIN_MATCH = 4,
    MAL_GEN_ENEMY_WIN_MATCH = 5,
    MAL_GEN_TIE_MATCH = 6,
    MAL_GEN_ALLY_NEW_LEAD = 7,
    MAL_GEN_ENEMY_NEW_LEAD = 8,
    MAL_GEN_FFA_NEW_LEAD = 9,
    MAL_GEN_TIME_FIVE_MINUTES = 10,
    MAL_GEN_TIME_ONE_MINUTE = 11,
    MAL_DM_INTRO = 12,
    MAL_TDM_INTRO = 13,
    MAL_DM_ALLY_WIN_NEAR = 14,
    MAL_DM_ENEMY_WIN_NEAR = 15,
    MAL_BAG_INTRO = 16,
    MAL_TBAG_INTRO = 17,
    MAL_BAG_BAG_DROPPED = 18,
    MAL_BAG_BAG_RESPAWNED = 19,
    MAL_BAG_ALLY_PICKUP = 20,
    MAL_BAG_ALLY_WIN_NEAR = 21,
    MAL_BAG_ENEMY_PICKUP = 22,
    MAL_BAG_ENEMY_WIN_NEAR = 23,
    MAL_CTF_INTRO = 24,
    MAL_CTF_ALLY_FLAG_CAPTURE = 25,
    MAL_CTF_ALLY_FLAG_DROP = 26,
    MAL_CTF_ALLY_FLAG_RETURN = 27,
    MAL_CTF_ALLY_FLAG_TAKEN = 28,
    MAL_CTF_ALLY_WIN_NEAR = 29,
    MAL_CTF_ENEMY_FLAG_CAPTURE = 30,
    MAL_CTF_ENEMY_FLAG_DROP = 31,
    MAL_CTF_ENEMY_FLAG_RETURN = 32,
    MAL_CTF_ENEMY_FLAG_TAKEN = 33,
    MAL_CTF_ENEMY_WIN_NEAR = 34,
    MAL_KOTH_INTRO = 35,
    MAL_KOTH_FRIENDLY_TARGET_DESTROYED = 36,
    MAL_KOTH_FRIENDLY_TARGET_REPAIRED = 37,
    MAL_KOTH_FRIENDLY_NEAR_WIN = 38,
    MAL_KOTH_ENEMY_TARGET_DESTROYED = 39,
    MAL_KOTH_ENEMY_TARGET_REPAIRED = 40,
    MAL_KOTH_ENEMY_NEAR_WIN = 41,
    MAL_DMO_INTRO = 42,
    MAL_DMO_ALLY_ENG_NEW = 43,
    MAL_DMO_ENEMY_ENG_NEW = 44,
    MAL_SGE_INTRO = 45,
    MAL_SGE_INFORM_OFFENSE = 46,
    MAL_SGE_INFORM_DEFENSE = 47,
    MAL_SGE_ALLY_TARGET_DESTROY = 48,
    MAL_SGE_ALLY_TARGET_WIN_NEAR = 49,
    MAL_SGE_ENEMY_TARGET_DESTROY = 50,
    MAL_SGE_ENEMY_TARGET_WIN_NEAR = 51,
    NUM_MULTI_ANNOUNCER_LINES = 52,
};

// size 0x20
struct multi_announcer_line_info
{
    multi_announcer_line_info(const char* name, const char* response_name, int min_time_between_plays_ms_in, dialogue_priorities priority_in, int supress_group); // 0x0094EE50 (:53)

    checksum_stri_pair line_name; // 0x00
    checksum_stri_pair team_response; // 0x08
    dialogue_priorities priority; // 0x10
    int supress_line_group; // 0x14
    int min_time_between_plays_ms; // 0x18
    timestamp last_played_time; // 0x1C
};

// size 0x8
struct pending_dialogue_info
{
    multi_announcer_lines line; // 0x00
    unsigned int handle; // 0x04
};

void unused_dcf_validate_mp_announce(); // 0x0094EEA0 (:176)
void multi_game_announcer_match_init(); // 0x0094F020 (:267)
void multi_game_announcer_round_init(); // 0x0094F040 (:283)
void multi_game_announcer_play_line(multi_announcer_lines line, unsigned int target_handle); // 0x0094F070 (:294)
bool multi_game_announcer_line_played_or_pending_this_round(multi_announcer_lines line); // 0x0094F120 (:331)
void multi_game_announcer_process(); // 0x0094F140 (:338)

// The announcer lines waiting for the dialogue manager (5 slots; multi_game_process also clears them).
extern pending_dialogue_info Pending_dialogues[5]; // 0x02FE738C
