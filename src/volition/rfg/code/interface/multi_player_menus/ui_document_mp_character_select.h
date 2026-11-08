// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_character_select.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_character_select.cpp
//
// Player and character select (the character preview through ui_character).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_player_select_init(); // 0x00914870 (:1164)

// UI.md UI5: the state callbacks are file statics of rfg.exe's interface unity (multi_player_menu_system_init
// registers them), external here.
void ui_document_mp_player_select_enter(game_state prev_state); // 0x0090B500 (:1039)
void ui_document_mp_player_select_exit(game_state new_state); // 0x008E0F40 (:1083)
void ui_document_mp_player_select_process(); // 0x009147F0 (:1104)
void ui_document_mp_player_select_handshake(rl_renderer*); // 0x008F21C0 (:1133)
void ui_document_mp_player_select_render(rl_renderer*); // 0x008FBA30 (:1141)
