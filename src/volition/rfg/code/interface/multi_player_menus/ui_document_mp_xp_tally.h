// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_xp_tally.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_xp_tally.cpp
//
// The XP tally after a match (ranks and unlocks).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

extern bool Xp_tally_connected_when_entered; // 0x016631C7
extern int Xp_tally_num_unlocks; // 0x02C036AC

void xp_tally_process_scroll_bar(); // 0x008D5E70 (:939)
void ui_document_mp_xp_tally_init(); // 0x009159B0 (:1435)

// UI.md UI5: the state callbacks are file statics of rfg.exe's interface unity (multi_player_menu_system_init
// registers them), external here.
void ui_document_mp_xp_tally_enter(game_state prev_state); // 0x0090EA30 (:1254)
void ui_document_mp_xp_tally_exit(game_state new_state); // 0x008F3AD0 (:1342)
void ui_document_mp_xp_tally_process(); // 0x009158C0 (:1356)
void ui_document_mp_xp_tally_handshake(rl_renderer*); // 0x008F3B70 (:1401)
void ui_document_mp_xp_tally_render(rl_renderer*); // 0x008FCEB0 (:1408)
