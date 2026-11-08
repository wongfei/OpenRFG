// Original: volition/rfg/code/hud/hud_ui_activity_complete.h
// Implementation: volition/rfg/code/hud/hud_ui_activity_complete.cpp
//
// The activity complete screen.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void hud_ui_activity_complete_init(); // 0x008686C0 (:751)
void hud_ui_activity_complete_enter(game_state prev_state); // 0x00846400 (:769)
void hud_ui_activity_complete_exit(game_state new_state); // 0x00846540 (:833)
void hud_ui_activity_complete_process(); // 0x00856530 (:865)
void hud_ui_activity_complete_handshake(rl_renderer*); // 0x008465D0 (:983)
void hud_ui_activity_complete_render(rl_renderer*); // 0x008465E0 (:989)
void hud_ui_activity_complete_do_frame(); // 0x00846620 (:1000)
void hud_ui_activity_complete_level_shutdown(); // 0x00846660 (:1011)
bool hud_ui_activity_complete_start(activity_modes mode, bool complete); // 0x00846670 (:1017)
void hud_ui_activity_complete_done(); // 0x00846870 (:1100)
void hud_ui_activity_complete_start_tally(int num); // 0x00846880 (:1106)
