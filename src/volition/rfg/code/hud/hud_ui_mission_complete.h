// Original: volition/rfg/code/hud/hud_ui_mission_complete.h
// Implementation: volition/rfg/code/hud/hud_ui_mission_complete.cpp
//
// The mission complete screen. Placement of mission_complete_result inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class mission_info;
class rl_renderer;

enum mission_complete_result
{
    MCR_NONE = 0,
    MCR_COMPLETED = 1,
    MCR_FAILED = 2,
    MCR_LAST = 3,
};

void hud_ui_mission_complete_init(); // 0x008695F0 (:540)
void hud_ui_mission_complete_enter(game_state prev_state); // 0x0084AAA0 (:558)
void hud_ui_mission_complete_exit(game_state new_state); // 0x0084ABD0 (:624)
void hud_ui_mission_complete_process(); // 0x00859400 (:795)
void hud_ui_mission_complete_handshake(rl_renderer*); // 0x0084AD40 (:901)
void hud_ui_mission_complete_render(rl_renderer*); // 0x0084AD50 (:910)
void hud_ui_mission_complete_set_subtitle(const char* subtitle); // 0x0084AD90 (:924)
void hud_ui_mission_complete_do_frame(); // 0x0084ADD0 (:934)
void hud_ui_mission_complete_level_shutdown(); // 0x0084AE10 (:945)
bool hud_ui_mission_complete_start(bool complete, const mission_info* const m_info); // 0x0084AE20 (:951)
void hud_ui_mission_complete_finished(); // 0x0084AF30 (:1006)
mission_complete_result hud_ui_get_mission_complete_result(); // 0x0084AF40 (:1012)
void hud_ui_mission_complete_start_tally(int num); // 0x0084AF50 (:1018)
