// Original: volition/rfg/code/cutscene/video_cutscene.h
// Implementation: volition/rfg/code/cutscene/video_cutscene.cpp
//
// Video cutscenes: full screen Bink movies played as a game state (with subtitles and optional credits), started by
// scripts.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

struct dlc_level_info;
class mempool_base;

extern float Video_cutscene_volume; // 0x0164BE94

bool video_cutscene_play_exit_notify(game_state new_state); // 0x00707140 (:330)
void unused_dcf_video_cutscene_play(); // 0x0070DA90 (:718)
void video_cutscene_system_init(); // 0x0070FE30 (:733)
void video_cutscene_play(const char* filename, unsigned int warp_to_object, const char* warp_to_zone, bool play_immediately, bool set_state, mempool_base* gpu_mempool, mempool_base* cpu_mempool, dlc_level_info* dlc_lvl); // 0x0070A540 (:750)
void video_cutscene_play_with_credits(const char* filename, unsigned int warp_to_object); // 0x0070A6B0 (:837)
bool video_cutscene_pending_or_playing(); // 0x007071E0 (:843)
void video_cutscene_do_frame(); // 0x00707210 (:850)
void video_cutscene_pause(); // 0x007072A0 (:906)
void video_cutscene_unpause(); // 0x007072D0 (:922)
